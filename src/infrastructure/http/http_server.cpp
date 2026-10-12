#include "infrastructure/http/http_server.hpp"

namespace http_server
{
void SessionBase::Run()
{
    net::dispatch(m_stream.get_executor(), beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}

void ReportError(const beast::error_code error, const std::string_view operation)
{
    logger::PrintLog(json::object{{"code", error.value()}, {"text", error.message()}, {"where", operation}}, "error");
}

SessionBase::SessionBase(tcp::socket&& socket):
        m_stream(std::move(socket))
{
}

void SessionBase::Read()
{
    m_request = {};
    m_stream.expires_after(m_request_timeout);
    http::async_read(m_stream, m_buffer, m_request, beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
}

void SessionBase::OnRead(const beast::error_code error, [[maybe_unused]] const std::size_t bytes_read)
{
    if (error == http::error::end_of_stream)
    {
        Close();
        return;
    }
    if (error)
    {
        ReportError(error, "read");
        return;
    }
    HandleRequest(std::move(m_request));
}

void SessionBase::Close()
{
    beast::error_code error;
    m_stream.socket().shutdown(tcp::socket::shutdown_send, error);
    if (error)
    {
        ReportError(error, "socket shutdown");
    }
}

void SessionBase::OnWrite(
        const bool close, const beast::error_code error, [[maybe_unused]] const std::size_t bytes_written)
{
    if (error)
    {
        ReportError(error, "write");
        return;
    }
    if (close)
    {
        Close();
        return;
    }
    Read();
}
} // namespace http_server
