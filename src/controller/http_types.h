#pragma once
#include "common/platform.h"
#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <filesystem>
#include <string_view>
#include <variant>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace fs = std::filesystem;
namespace sys = boost::system;
namespace net = boost::asio;

using namespace std::literals;

using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;
using FileResponse = http::response<http::file_body>;
using ApiRequestResult = StringResponse;
using FileRequestResult = std::variant<StringResponse, FileResponse>;

struct ContentType {
    ContentType() = delete;

    constexpr static std::string_view TEXT_HTML = "text/html"sv;
    constexpr static std::string_view TEXT_CSS = "text/css"sv;
    constexpr static std::string_view TEXT_PLAIN = "text/plain"sv;
    constexpr static std::string_view TEXT_JAVASCRIPT = "text/javascript"sv;

    constexpr static std::string_view APP_JSON = "application/json"sv;
    constexpr static std::string_view APP_XML = "application/xml"sv;
    constexpr static std::string_view APP_OCTET_STREAM = "application/octet-stream"sv;

    constexpr static std::string_view IMG_PNG = "image/png"sv;
    constexpr static std::string_view IMG_JPEG = "image/jpeg"sv;
    constexpr static std::string_view IMG_GIF = "image/gif"sv;
    constexpr static std::string_view IMG_BMP = "image/bmp"sv;
    constexpr static std::string_view IMG_ICO = "image/vnd.microsoft.icon"sv;
    constexpr static std::string_view IMG_TIFF = "image/tiff"sv;
    constexpr static std::string_view IMG_SVG = "image/svg+xml"sv;

    constexpr static std::string_view AUDIO_MPEG = "audio/mpeg"sv;
};

enum class ApiRequestType {
    MapsList,
    MapDescription,
    JoinGame,
    PlayersList,
    GameState,
    Action,
    Tick,
    GameRecords,
    Invalid
};

} // namespace http_handler
