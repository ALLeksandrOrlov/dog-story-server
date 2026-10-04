#include <charconv>

#include "common/string_processing.h"
#include "controller/request_handler.h"

namespace http_handler {

std::string RequestHandler::DecodeURI(std::string_view target) {
    std::string decoded;
    decoded.reserve(target.size());
    for (size_t i = 0; i < target.size(); ++i) {
        char ch = target[i];
        if (ch == '%') {
            if (i + 2 >= target.size())
                throw InvalidTarget("Incomplete percent escape");
            unsigned value = 0;
            const char* first = target.data() + i + 1;
            const auto result = std::from_chars(first, first + 2, value, 16);
            if (result.ec != std::errc{} || result.ptr != first + 2) {
                throw InvalidTarget("Invalid percent escape");
            }
            ch = static_cast<char>(value);
            i += 2;
        } else if (ch == '+') {
            ch = ' ';
        }
        if (ch == '\0')
            throw InvalidTarget("NUL is not allowed in a request target");
        decoded += ch;
    }
    return decoded;
}

bool RequestHandler::IsApiRequest(std::string_view target) {
    auto target_items = Split(target, '/');
    if ((target_items.size() > 0) && (target_items[0] == "api"s)) {
        return true;
    }

    return false;
}

StringResponse RequestHandler::ReportServerError(const StringRequest& req, std::string_view message) const {
    if (report_error_)
        report_error_(message);
    ApiRequestResult response;
    response.result(http::status::internal_server_error);
    response.version(req.version());
    response.set(http::field::content_type, "text/plain"sv);
    response.body() = "Internal server error";
    response.prepare_payload();
    return response;
}

} // namespace http_handler
