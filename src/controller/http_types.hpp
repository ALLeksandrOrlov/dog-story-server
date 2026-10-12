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

/** @brief MIME-типы ресурсов, распознаваемые статическим сервером. */
struct ContentType
{
    /** @brief Набор констант не требует экземпляра. */
    ContentType() = delete;

    /** @brief Тип содержимого text/html. */
    constexpr static std::string_view text_html = "text/html"sv;
    /** @brief Тип содержимого text/css. */
    constexpr static std::string_view text_css = "text/css"sv;
    /** @brief Тип содержимого text/plain. */
    constexpr static std::string_view text_plain = "text/plain"sv;
    /** @brief Тип содержимого text/javascript. */
    constexpr static std::string_view text_javascript = "text/javascript"sv;

    /** @brief Тип содержимого application/json. */
    constexpr static std::string_view app_json = "application/json"sv;
    /** @brief Тип содержимого application/xml. */
    constexpr static std::string_view app_xml = "application/xml"sv;
    /** @brief Тип содержимого application/octet-stream. */
    constexpr static std::string_view app_octet_stream = "application/octet-stream"sv;

    /** @brief Тип содержимого image/png. */
    constexpr static std::string_view img_png = "image/png"sv;
    /** @brief Тип содержимого image/jpeg. */
    constexpr static std::string_view img_jpeg = "image/jpeg"sv;
    /** @brief Тип содержимого image/gif. */
    constexpr static std::string_view img_gif = "image/gif"sv;
    /** @brief Тип содержимого image/bmp. */
    constexpr static std::string_view img_bmp = "image/bmp"sv;
    /** @brief Тип содержимого image/vnd.microsoft.icon. */
    constexpr static std::string_view img_ico = "image/vnd.microsoft.icon"sv;
    /** @brief Тип содержимого image/tiff. */
    constexpr static std::string_view img_tiff = "image/tiff"sv;
    /** @brief Тип содержимого image/svg+xml. */
    constexpr static std::string_view img_svg = "image/svg+xml"sv;

    /** @brief Тип содержимого audio/mpeg. */
    constexpr static std::string_view audio_mpeg = "audio/mpeg"sv;
};

/** @brief Внутренняя классификация маршрутов версии v1. */
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
