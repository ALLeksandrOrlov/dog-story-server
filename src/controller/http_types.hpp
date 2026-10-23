#pragma once
#include <filesystem>
#include <string_view>
#include <variant>

#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include "common/platform.hpp"

namespace http_handler
{
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

/** @brief Resource MIME types recognized by the static server. */
struct ContentType
{
    /** @brief This set of constants needs no instance. */
    ContentType() = delete;

    /** @brief Content type: text/html. */
    constexpr static std::string_view text_html = "text/html"sv;
    /** @brief Content type: text/css. */
    constexpr static std::string_view text_css = "text/css"sv;
    /** @brief Content type: text/plain. */
    constexpr static std::string_view text_plain = "text/plain"sv;
    /** @brief Content type: text/javascript. */
    constexpr static std::string_view text_javascript = "text/javascript"sv;

    /** @brief Content type: application/json. */
    constexpr static std::string_view app_json = "application/json"sv;
    /** @brief Content type: application/xml. */
    constexpr static std::string_view app_xml = "application/xml"sv;
    /** @brief Content type: application/octet-stream. */
    constexpr static std::string_view app_octet_stream = "application/octet-stream"sv;

    /** @brief Content type: image/png. */
    constexpr static std::string_view img_png = "image/png"sv;
    /** @brief Content type: image/jpeg. */
    constexpr static std::string_view img_jpeg = "image/jpeg"sv;
    /** @brief Content type: image/gif. */
    constexpr static std::string_view img_gif = "image/gif"sv;
    /** @brief Content type: image/bmp. */
    constexpr static std::string_view img_bmp = "image/bmp"sv;
    /** @brief Content type: image/vnd.microsoft.icon. */
    constexpr static std::string_view img_ico = "image/vnd.microsoft.icon"sv;
    /** @brief Content type: image/tiff. */
    constexpr static std::string_view img_tiff = "image/tiff"sv;
    /** @brief Content type: image/svg+xml. */
    constexpr static std::string_view img_svg = "image/svg+xml"sv;

    /** @brief Content type: audio/mpeg. */
    constexpr static std::string_view audio_mpeg = "audio/mpeg"sv;
};

/** @brief Internal classification of v1 routes. */
enum class ApiRequestType
{
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
