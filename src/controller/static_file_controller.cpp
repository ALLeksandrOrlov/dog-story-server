#include "controller/static_file_controller.h"
#include "common/string_processing.h"
#include <algorithm>
#include <cctype>

namespace http_handler {
using namespace http;

FileRequestResult StaticFileController::Handle(const StringRequest& req, std::string_view uri) {
    auto uri_items = Split(uri, '/');

    fs::path uri_path{static_path_};
    for (auto item : uri_items) {
        uri_path /= item;
    }

    if (!fs::exists(uri_path)) {
        ApiRequestResult response;
        response.result(status::not_found);
        response.insert(field::content_type, ContentType::TEXT_PLAIN);
        return response;
    }

    if (!IsSubPath(uri_path, static_path_)) {
        ApiRequestResult response;
        response.result(status::bad_request);
        response.insert(field::content_type, ContentType::TEXT_PLAIN);
        return response;
    }

    if (fs::is_directory(uri_path)) {
        fs::path index_html_path{uri_path / "index.html"s};
        if (!fs::exists(index_html_path)) {
            ApiRequestResult response;
            response.result(status::not_found);
            response.insert(field::content_type, ContentType::TEXT_PLAIN);
            return response;
        }

        if (!IsSubPath(index_html_path, static_path_)) {
            StringResponse response;
            response.result(status::bad_request);
            response.set(field::content_type, ContentType::TEXT_PLAIN);
            return response;
        }

        response<file_body> response;
        response.version(11);
        response.result(status::ok);
        response.insert(field::content_type, ContentType::TEXT_HTML);

        file_body::value_type file;
        if (sys::error_code ec; file.open(index_html_path.c_str(), beast::file_mode::read, ec), ec) {
            throw std::runtime_error("Unable to open file"s);
        }

        response.body() = std::move(file);
        response.prepare_payload();
        return response;
    }

    response<file_body> response;
    response.version(11);
    response.result(status::ok);
    response.insert(field::content_type, GetFileContentType(uri_path));

    file_body::value_type file;
    if (sys::error_code ec; file.open(uri_path.c_str(), beast::file_mode::read, ec), ec) {
        throw std::runtime_error("Unable to open file"s);
    }

    response.body() = std::move(file);
    response.prepare_payload();
    return response;
}

std::string_view StaticFileController::GetFileContentType(const fs::path& file_path) {
    std::string ext = file_path.extension().string();

    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

    if ((ext == ".htm"sv) || (ext == ".html"sv)) {
        return ContentType::TEXT_HTML;
    }

    if (ext == ".css"sv) {
        return ContentType::TEXT_CSS;
    }

    if (ext == ".txt"sv) {
        return ContentType::TEXT_PLAIN;
    }

    if (ext == ".js"sv) {
        return ContentType::TEXT_JAVASCRIPT;
    }

    if (ext == ".json"sv) {
        return ContentType::APP_JSON;
    }

    if (ext == ".xml"sv) {
        return ContentType::APP_XML;
    }

    if (ext == ".png"sv) {
        return ContentType::IMG_PNG;
    }

    if ((ext == ".jpg"sv) || (ext == ".jpe"sv) || (ext == ".jpeg"sv)) {
        return ContentType::IMG_JPEG;
    }

    if (ext == ".gif"sv) {
        return ContentType::IMG_GIF;
    }

    if (ext == ".bmp"sv) {
        return ContentType::IMG_BMP;
    }

    if (ext == ".ico"sv) {
        return ContentType::IMG_ICO;
    }

    if ((ext == ".tiff"sv) || (ext == ".tif"sv)) {
        return ContentType::IMG_TIFF;
    }

    if ((ext == ".svg"sv) || (ext == ".svgz"sv)) {
        return ContentType::IMG_SVG;
    }

    if (ext == ".mp3"sv) {
        return ContentType::AUDIO_MPEG;
    }

    return ContentType::APP_OCTET_STREAM;
}

bool StaticFileController::IsSubPath(fs::path path, fs::path base) {
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);

    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

} // namespace http_handler
