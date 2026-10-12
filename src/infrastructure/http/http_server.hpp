#pragma once

#include <chrono>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

#include <boost/asio/dispatch.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include "common/platform.hpp"
#include "infrastructure/logging/logger.hpp"

namespace http_server
{
namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;
namespace json = boost::json;

/** @brief Записывает код и место сетевой ошибки в структурированный журнал. */
void ReportError(const beast::error_code error, const std::string_view operation);

/**
 * @brief Владеет TCP-соединением и последовательно читает запросы и отправляет ответы.
 * @details Все операции сокета выполняются на его strand. Асинхронные обработчики
 * удерживают shared_ptr, поэтому соединение живёт до завершения последней операции.
 */
class SessionBase
{
public:
    /** @brief Сессия имеет единственного логического владельца сокета. */
    SessionBase(const SessionBase&) = delete;
    /** @brief Перемещение запрещено после привязки асинхронных обработчиков. */
    SessionBase(SessionBase&&) = delete;
    /** @brief Копирование сокета запрещено. */
    SessionBase& operator=(const SessionBase&) = delete;
    /** @brief Перенос активных обработчиков запрещён. */
    SessionBase& operator=(SessionBase&&) = delete;
    /** @brief Планирует первое чтение на исполнителе сокета. */
    void Run();

protected:
    /** @brief HTTP-запрос с текстовым телом для передачи прикладному обработчику. */
    using HttpRequest = http::request<http::string_body>;

    /** @brief Принимает владение сокетом, уже связанным со своим strand. */
    explicit SessionBase(tcp::socket&& socket);
    /** @brief Освобождает сетевые ресурсы при уничтожении конкретной сессии. */
    virtual ~SessionBase() = default;

    /** @brief Удерживает тело ответа в памяти и отправляет его на исполнителе сокета. */
    template <typename Body, typename Fields> void Write(http::response<Body, Fields>&& response)
    {
        const auto saved_response = std::make_shared<http::response<Body, Fields>>(std::move(response));
        const auto self = GetSharedThis();
        net::dispatch(m_stream.get_executor(),
                [saved_response, self]
                {
                    http::async_write(self->m_stream, *saved_response,
                            [saved_response, self](const beast::error_code error, const std::size_t bytes_written)
                            {
                                self->OnWrite(saved_response->need_eof(), error, bytes_written);
                            });
                });
    }

    /** @brief Владеющий TCP-поток с таймаутом; доступ выполняется только на его strand. */
    beast::tcp_stream m_stream;

private:
    /** @brief Максимальное ожидание чтения одного HTTP-запроса. */
    static constexpr auto m_request_timeout = std::chrono::seconds(30);
    /** @brief Непотреблённые байты соединения, в том числе начало следующего запроса. */
    beast::flat_buffer m_buffer;
    /** @brief Текущий запрос до передачи обработчику приложения. */
    HttpRequest m_request;

    /** @brief Начинает чтение следующего HTTP-запроса с ограничением времени. */
    void Read();
    /** @brief Передаёт полный запрос адаптеру либо завершает соединение при сетевой ошибке. */
    void OnRead(const beast::error_code error, [[maybe_unused]] const std::size_t bytes_read);
    /** @brief Завершает отправку данных в TCP-соединении. */
    void Close();
    /** @brief Передаёт владение запросом конкретному обработчику приложения. */
    virtual void HandleRequest(HttpRequest&& request) = 0;
    /** @brief Возвращает владеющую ссылку для продолжения асинхронной операции. */
    virtual std::shared_ptr<SessionBase> GetSharedThis() = 0;
    /** @brief После ответа закрывает соединение либо начинает следующее чтение. */
    void OnWrite(const bool close, const beast::error_code error, [[maybe_unused]] const std::size_t bytes_written);
};

/** @brief Связывает жизненный цикл HTTP-соединения с обработчиком запросов и журналированием. */
template <typename RequestHandler>
class Session : public SessionBase, public std::enable_shared_from_this<Session<RequestHandler>>
{
public:
    /** @brief Принимает сокет и сохраняет обработчик запросов по значению. */
    template <typename Handler>
    Session(tcp::socket&& socket, Handler&& request_handler):
            SessionBase(std::move(socket)),
            m_request_handler(std::forward<Handler>(request_handler))
    {
    }

private:
    /** @brief Сохранённый по значению обработчик передачи запроса прикладному слою. */
    RequestHandler m_request_handler;

    /** @brief Продлевает жизнь сессии до завершения сетевой операции. */
    std::shared_ptr<SessionBase> GetSharedThis() override
    {
        return this->shared_from_this();
    }

    /** @brief Измеряет время обработки и передаёт ответ в очередь отправки сокета. */
    void HandleRequest(HttpRequest&& request) override
    {
        const auto remote_endpoint = m_stream.socket().remote_endpoint();
        logger::PrintLog(json::object{{"ip", remote_endpoint.address().to_string()}, {"URI", request.target()},
                                 {"method", request.method_string()}},
                "request received");
        const auto begin = std::chrono::steady_clock::now();
        m_request_handler(std::move(request),
                [self = this->shared_from_this(), begin](auto&& response)
                {
                    const auto elapsed = std::chrono::steady_clock::now() - begin;
                    const auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
                    const auto content_type = response.find(http::field::content_type);
                    const std::string_view type = (content_type == response.end()) ? "null" : content_type->value();
                    logger::PrintLog(json::object{{"response_time", std::to_string(response_time.count())},
                                             {"code", response.result_int()}, {"content_type", type}},
                            "response sent");
                    self->Write(std::move(response));
                });
    }
};

/**
 * @brief Принимает TCP-соединения и создаёт для каждого отдельную HTTP-сессию.
 * @details Ошибки приёма повторяются после задержки; отмена при остановке не запускает приём заново.
 */
template <typename RequestHandler> class Listener : public std::enable_shared_from_this<Listener<RequestHandler>>
{
public:
    /** @brief Открывает endpoint и сохраняет обработчик; ошибки привязки завершают запуск. */
    template <typename Handler>
    Listener(net::io_context& context, const tcp::endpoint& endpoint, Handler&& request_handler):
            m_context(context),
            m_acceptor(net::make_strand(context)),
            m_retry_timer(m_acceptor.get_executor()),
            m_request_handler(std::forward<Handler>(request_handler))
    {
        m_acceptor.open(endpoint.protocol());
        m_acceptor.set_option(net::socket_base::reuse_address(true));
        m_acceptor.bind(endpoint);
        m_acceptor.listen(net::socket_base::max_listen_connections);
    }

    /** @brief Запускает приём на strand слушателя; вызывается один раз после создания shared_ptr. */
    void Run()
    {
        net::dispatch(m_acceptor.get_executor(),
                [self = this->shared_from_this()]
                {
                    self->AcceptNext();
                });
    }

private:
    /** @brief Пауза между неуспешными попытками приёма, предотвращающая горячий цикл. */
    static constexpr auto m_retry_delay = std::chrono::milliseconds(100);
    /** @brief Заимствованный контекст исполнения, переживающий слушатель и сессии. */
    net::io_context& m_context;
    /** @brief Владеющий принимающий сокет со своим последовательным исполнителем. */
    tcp::acceptor m_acceptor;
    /** @brief Таймер повторного приёма после ошибки; использует strand слушателя. */
    net::steady_timer m_retry_timer;
    /** @brief Сохранённый по значению обработчик передачи запроса прикладному слою. */
    RequestHandler m_request_handler;

    /** @brief Принимает один сокет, выделяя ему отдельный strand. */
    void AcceptNext()
    {
        m_acceptor.async_accept(
                net::make_strand(m_context), beast::bind_front_handler(&Listener::OnAccept, this->shared_from_this()));
    }

    /** @brief После временной ошибки откладывает следующую попытку, исключая горячий цикл. */
    void RetryAccept()
    {
        m_retry_timer.expires_after(m_retry_delay);
        m_retry_timer.async_wait(
                [self = this->shared_from_this()](const sys::error_code error)
                {
                    if (!error && self->m_acceptor.is_open())
                    {
                        self->AcceptNext();
                    }
                });
    }

    /** @brief Создаёт сессию; после ошибки сохраняет работоспособность слушателя. */
    void OnAccept(const sys::error_code error, tcp::socket socket)
    {
        if (error)
        {
            if (error != net::error::operation_aborted && m_acceptor.is_open())
            {
                ReportError(error, "accept");
                RetryAccept();
            }
            return;
        }
        std::make_shared<Session<RequestHandler>>(std::move(socket), m_request_handler)->Run();
        AcceptNext();
    }
};

/** @brief Создаёт самоподдерживающийся слушатель; context должен пережить все сетевые операции. */
template <typename RequestHandler>
void ServeHttp(net::io_context& context, const tcp::endpoint& endpoint, RequestHandler&& handler)
{
    using HttpListener = Listener<std::decay_t<RequestHandler>>;
    std::make_shared<HttpListener>(context, endpoint, std::forward<RequestHandler>(handler))->Run();
}
} // namespace http_server
