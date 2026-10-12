#include <charconv>

#include "common/string_processing.hpp"

#include "controller/request_handler.hpp"

namespace http_handler
{
namespace
{
/** @brief Число шестнадцатеричных цифр в percent-escape. */
constexpr std::size_t escape_digits = 2;
/** @brief Основание системы счисления URI escape. */
constexpr int hex_base = 16;
} // namespace

std::string RequestHandler::DecodeURI(const std::string_view target)
{
    std::string decoded;
    decoded.reserve(target.size());
    for(size_t i = 0; i < target.size(); ++i)
    {
        char ch = target[i];
        if (ch == '%')
        {
            if (i + escape_digits >= target.size())
            {
                throw InvalidTarget("Incomplete percent escape");
            }
            unsigned value = 0;
            const char* first = target.data() + i + 1;
            const auto result = std::from_chars(first, first + escape_digits, value, hex_base);
            if (result.ec != std::errc{} || result.ptr != first + escape_digits)
            {
                throw InvalidTarget("Invalid percent escape");
            }
            ch = static_cast<char>(value);
            /** @brief Число шестнадцатеричных цифр в percent-escape. */
            i += escape_digits;
        }
        else if (ch == '+')
        {
            ch = ' ';
        }
        if (ch == '\0')
        {
            throw InvalidTarget("NUL is not allowed in a request target");
        }
        decoded += ch;
    }
    return decoded;
}

bool RequestHandler::IsApiRequest(const std::string_view target)
{
    const auto target_items = util::Split(target, '/');
    return !target_items.empty() && target_items.front() == "api";
}

StringResponse RequestHandler::ReportServerError(const StringRequest& req, const std::string_view message) const
{
    if (m_report_error)
    {
        m_report_error(message);
    }
    ApiRequestResult response;
    response.result(http::status::internal_server_error);
    response.version(req.version());
    response.set(http::field::content_type, "text/plain"sv);
    response.body() = "Internal server error";
    response.prepare_payload();
    return response;
}

} // namespace http_handler
