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

/** @brief Writes the network error code and operation to the structured log. */
void ReportError(const beast::error_code error, const std::string_view operation);

/**
 * @brief Owns a TCP connection and reads requests and sends responses sequentially.
 * @details All socket operations run on its strand. Asynchronous handlers
 * retain a shared_ptr, keeping the connection alive until the last operation completes.
 */
class SessionBase
{
public:
    /** @brief The session is the sole logical owner of the socket. */
    SessionBase(const SessionBase&) = delete;
    /** @brief Disables moves once asynchronous handlers have been bound. */
    SessionBase(SessionBase&&) = delete;
    /** @brief Disables copying the socket. */
    SessionBase& operator=(const SessionBase&) = delete;
    /** @brief Disables moving active handlers. */
    SessionBase& operator=(SessionBase&&) = delete;
    /** @brief Schedules the first read on the socket executor. */
    void Run();

protected:
    /** @brief HTTP request with a text body passed to the application handler. */
    using HttpRequest = http::request<http::string_body>;

    /** @brief Takes ownership of a socket already bound to its own strand. */
    explicit SessionBase(tcp::socket&& socket);
    /** @brief Releases network resources when the concrete session is destroyed. */
    virtual ~SessionBase() = default;

    /** @brief Keeps the response body alive and sends it on the socket executor. */
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

    /** @brief Owned TCP stream with a timeout; accessed only on its strand. */
    beast::tcp_stream m_stream;

private:
    /** @brief Maximum time allowed to read one HTTP request. */
    static constexpr auto m_request_timeout = std::chrono::seconds(30);
    /** @brief Unconsumed connection bytes, including the beginning of the next request. */
    beast::flat_buffer m_buffer;
    /** @brief Current request before transfer to the application handler. */
    HttpRequest m_request;

    /** @brief Starts reading the next HTTP request with a time limit. */
    void Read();
    /** @brief Passes a complete request to the adapter or ends the connection on a network error. */
    void OnRead(const beast::error_code error, [[maybe_unused]] const std::size_t bytes_read);
    /** @brief Shuts down the sending side of the TCP connection. */
    void Close();
    /** @brief Transfers request ownership to the concrete application handler. */
    virtual void HandleRequest(HttpRequest&& request) = 0;
    /** @brief Returns an owning reference to continue an asynchronous operation. */
    virtual std::shared_ptr<SessionBase> GetSharedThis() = 0;
    /** @brief Closes the connection or starts the next read after a response. */
    void OnWrite(const bool close, const beast::error_code error, [[maybe_unused]] const std::size_t bytes_written);
};

/** @brief Connects the HTTP connection lifecycle to request handling and logging. */
template <typename RequestHandler>
class Session : public SessionBase, public std::enable_shared_from_this<Session<RequestHandler>>
{
public:
    /** @brief Accepts a socket and stores the request handler by value. */
    template <typename Handler>
    Session(tcp::socket&& socket, Handler&& request_handler):
            SessionBase(std::move(socket)),
            m_request_handler(std::forward<Handler>(request_handler))
    {
    }

private:
    /** @brief Handler stored by value for passing requests to the application layer. */
    RequestHandler m_request_handler;

    /** @brief Keeps the session alive until the network operation completes. */
    std::shared_ptr<SessionBase> GetSharedThis() override
    {
        return this->shared_from_this();
    }

    /** @brief Measures processing time and queues the response on the socket executor. */
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
 * @brief Accepts TCP connections and creates a separate HTTP session for each.
 * @details Retries failed accepts after a delay; cancellation during shutdown does not restart acceptance.
 */
template <typename RequestHandler> class Listener : public std::enable_shared_from_this<Listener<RequestHandler>>
{
public:
    /** @brief Opens the endpoint and stores the handler; bind errors abort startup. */
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

    /** @brief Starts accepting on the listener strand; called once after shared_ptr creation. */
    void Run()
    {
        net::dispatch(m_acceptor.get_executor(),
                [self = this->shared_from_this()]
                {
                    self->AcceptNext();
                });
    }

private:
    /** @brief Delay between failed accept attempts to prevent a busy loop. */
    static constexpr auto m_retry_delay = std::chrono::milliseconds(100);
    /** @brief Borrowed execution context that outlives the listener and sessions. */
    net::io_context& m_context;
    /** @brief Owned acceptor socket with its own sequential executor. */
    tcp::acceptor m_acceptor;
    /** @brief Timer for retrying a failed accept; uses the listener strand. */
    net::steady_timer m_retry_timer;
    /** @brief Handler stored by value for passing requests to the application layer. */
    RequestHandler m_request_handler;

    /** @brief Accepts one socket, assigning it a separate strand. */
    void AcceptNext()
    {
        m_acceptor.async_accept(
                net::make_strand(m_context), beast::bind_front_handler(&Listener::OnAccept, this->shared_from_this()));
    }

    /** @brief Delays the next attempt after a transient error to prevent a busy loop. */
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

    /** @brief Creates a session; keeps the listener operational after an error. */
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

/** @brief Creates a listener that manages its own lifetime; context must outlive all network operations. */
template <typename RequestHandler>
void ServeHttp(net::io_context& context, const tcp::endpoint& endpoint, RequestHandler&& handler)
{
    using HttpListener = Listener<std::decay_t<RequestHandler>>;
    std::make_shared<HttpListener>(context, endpoint, std::forward<RequestHandler>(handler))->Run();
}
} // namespace http_server
