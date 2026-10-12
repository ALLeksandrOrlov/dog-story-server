#pragma once
#include <functional>

#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>

#include "controller/api_controller.hpp"
#include "controller/static_file_controller.hpp"

namespace http_handler
{
/**
 * @brief Разделяет запросы к API и статическим файлам и изолирует исключения HTTP-границей.
 * @details Объект создаётся через shared_ptr: отложенный вызов удерживает обработчик до отправки ответа.
 * API исполняется в переданном strand; статические файлы обрабатываются в контексте сокета.
 */
class RequestHandler : public std::enable_shared_from_this<RequestHandler>
{
public:
    /** @brief Приёмник диагностических сообщений о внутренних ошибках. */
    using ErrorReporter = std::function<void(std::string_view)>;
    /** @brief Последовательный исполнитель прикладных операций. */
    using Strand = net::strand<net::io_context::executor_type>;

    /** @brief Заимствует Application и JsonView; они должны пережить все незавершённые запросы. */
    RequestHandler(const std::string_view static_path, const Strand api_strand, app::Application& app,
            const view::JsonView& view, ErrorReporter report_error = {}):
            m_api(app, view),
            m_static_files(static_path),
            m_api_strand(api_strand),
            m_report_error(std::move(report_error))
    {
    }

    /** @brief Запрещает копирование обработчика с незавершёнными операциями. */
    RequestHandler(const RequestHandler&) = delete;
    /** @brief Запрещает замену зависимостей обработчика после запуска. */
    RequestHandler& operator=(const RequestHandler&) = delete;

    /**
     * @brief Передаёт запрос контроллеру и вызывает отправку ровно одного результата.
     * @param request Запрос передаётся во владение отложенной операции API.
     * @param send Обработчик отправки, поддерживающий строковый и файловый ответ.
     * @details Некорректный URI даёт 400, исключения прикладного слоя и инфраструктуры — 500.
     */
    template <typename Send> void operator()(StringRequest&& request, Send&& send)
    {
        const std::string_view target = request.target();

        try
        {
            // Separate the query before decoding, so encoded '?' remains part of a filename.
            std::string uri = DecodeURI(target.substr(0, target.find('?')));
            if (IsApiRequest(uri))
            {
                uri = DecodeURI(target);
                auto handle = [self = shared_from_this(), send, request = std::forward<decltype(request)>(request), uri]
                {
                    try
                    {
                        // Изменения игры сериализуются независимо от strand клиентского сокета.
                        assert(self->m_api_strand.running_in_this_thread());
                        return send(self->m_api.Handle(request, uri));
                    }
                    catch(const std::exception& error)
                    {
                        send(self->ReportServerError(request, error.what()));
                    }
                };
                return net::dispatch(m_api_strand, handle);
            }
            // Возвращаем результат обработки запроса к файлу
            return std::visit(
                    [&send](auto&& result)
                    {
                        send(std::forward<decltype(result)>(result));
                    },
                    m_static_files.Handle(request, uri));
        }
        catch(const InvalidTarget&)
        {
            StringResponse response{http::status::bad_request, request.version()};
            response.set(http::field::content_type, "application/json");
            response.body() = R"({"code":"badRequest","message":"Invalid request target"})";
            response.prepare_payload();
            send(std::move(response));
        }
        catch(const std::exception& error)
        {
            send(ReportServerError(request, error.what()));
        }
    }

private:
    /** @brief Ошибка синтаксиса адреса, отличаемая от внутренних отказов обработки. */
    class InvalidTarget : public std::invalid_argument
    {
    public:
        using std::invalid_argument::invalid_argument;
    };

    /** @brief Декодирует percent-escape; отклоняет неполные последовательности и NUL. */
    static std::string DecodeURI(const std::string_view target);

    /** @brief Проверяет первый сегмент пути на префикс API. */
    static bool IsApiRequest(const std::string_view target);

    /** @brief Журналирует причину и возвращает 500 без раскрытия внутренних подробностей клиенту. */
    StringResponse ReportServerError(const StringRequest& request, const std::string_view message) const;

    /** @brief Контроллер игровых HTTP-маршрутов. */
    ApiController m_api;
    /** @brief Контроллер файлов в корневом каталоге клиента. */
    StaticFileController m_static_files;
    /** @brief Общий с таймером исполнитель, защищающий состояние игры. */
    Strand m_api_strand;
    /** @brief Необязательный приёмник диагностических сообщений. */
    ErrorReporter m_report_error;
};

} // namespace http_handler
