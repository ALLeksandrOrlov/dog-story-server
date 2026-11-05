#include <algorithm>
#include <cctype>
#include <utility>

#include "common/string_processing.hpp"

#include "controller/static_file_controller.hpp"

namespace http_handler
{
using namespace http;
namespace
{
/** @brief HTTP/1.1 version number as represented by Boost.Beast. */
constexpr unsigned http_version = 11;

/** @brief Supported file extensions, including aliases, and their response MIME types. */
constexpr std::pair<std::string_view, std::string_view> content_types[] = {
        {".htm", ContentType::text_html},
        {".html", ContentType::text_html},
        {".css", ContentType::text_css},
        {".txt", ContentType::text_plain},
        {".js", ContentType::text_javascript},
        {".json", ContentType::app_json},
        {".xml", ContentType::app_xml},
        {".png", ContentType::img_png},
        {".jpg", ContentType::img_jpeg},
        {".jpe", ContentType::img_jpeg},
        {".jpeg", ContentType::img_jpeg},
        {".gif", ContentType::img_gif},
        {".bmp", ContentType::img_bmp},
        {".ico", ContentType::img_ico},
        {".tiff", ContentType::img_tiff},
        {".tif", ContentType::img_tiff},
        {".svg", ContentType::img_svg},
        {".svgz", ContentType::img_svg},
        {".mp3", ContentType::audio_mpeg},
};
} // namespace

FileRequestResult StaticFileController::Handle(const StringRequest& /*request*/, const std::string_view uri) const
{
    const auto uri_items = util::Split(uri, '/');

    fs::path uri_path{m_static_path};
    for(const auto item : uri_items)
    {
        uri_path /= item;
    }

    if (!fs::exists(uri_path))
    {
        ApiRequestResult response;
        response.result(status::not_found);
        response.insert(field::content_type, ContentType::text_plain);
        return response;
    }

    if (!IsSubPath(uri_path, m_static_path))
    {
        ApiRequestResult response;
        response.result(status::bad_request);
        response.insert(field::content_type, ContentType::text_plain);
        return response;
    }

    if (fs::is_directory(uri_path))
    {
        fs::path index_html_path{uri_path / "index.html"s};
        if (!fs::exists(index_html_path))
        {
            ApiRequestResult response;
            response.result(status::not_found);
            response.insert(field::content_type, ContentType::text_plain);
            return response;
        }

        if (!IsSubPath(index_html_path, m_static_path))
        {
            StringResponse response;
            response.result(status::bad_request);
            response.set(field::content_type, ContentType::text_plain);
            return response;
        }

        response<file_body> response;
        response.version(http_version);
        response.result(status::ok);
        response.insert(field::content_type, ContentType::text_html);

        file_body::value_type file;
        sys::error_code error;
        file.open(index_html_path.c_str(), beast::file_mode::read, error);
        if (error)
        {
            throw std::runtime_error("Unable to open file"s);
        }

        response.body() = std::move(file);
        response.prepare_payload();
        return response;
    }

    response<file_body> response;
    response.version(http_version);
    response.result(status::ok);
    response.insert(field::content_type, GetFileContentType(uri_path));

    file_body::value_type file;
    sys::error_code error;
    file.open(uri_path.c_str(), beast::file_mode::read, error);
    if (error)
    {
        throw std::runtime_error("Unable to open file"s);
    }

    response.body() = std::move(file);
    response.prepare_payload();
    return response;
}

std::string_view StaticFileController::GetFileContentType(const fs::path& file_path)
{
    std::string ext = file_path.extension().string();

    std::transform(ext.begin(), ext.end(), ext.begin(),
            [](const unsigned char c)
            {
                return std::tolower(c);
            });

    for(const auto& [extension, content_type] : content_types)
    {
        if (ext == extension)
        {
            return content_type;
        }
    }

    return ContentType::app_octet_stream;
}

bool StaticFileController::IsSubPath(const fs::path& path, const fs::path& base)
{
    const auto canonical_path = fs::weakly_canonical(path);
    const auto canonical_base = fs::weakly_canonical(base);

    auto path_component = canonical_path.begin();
    for(auto base_component = canonical_base.begin(); base_component != canonical_base.end();
            ++base_component, ++path_component)
    {
        if (path_component == canonical_path.end() || *path_component != *base_component)
        {
            return false;
        }
    }
    return true;
}

} // namespace http_handler
