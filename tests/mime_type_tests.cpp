#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>

#include "controller/static_file_controller.hpp"
#include "domain/retired_player.hpp"

namespace
{
/** @brief Owns an isolated directory for exercising public static file responses. */
class TemporaryDirectory
{
public:
    /** @brief Creates an empty directory with a unique name. */
    TemporaryDirectory():
            m_path(std::filesystem::temp_directory_path() / ("mime-types-" + domain::RetiredPlayerId::New().ToString()))
    {
        std::filesystem::create_directory(m_path);
    }

    /** @brief Removes test files without throwing during stack unwinding. */
    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    TemporaryDirectory(TemporaryDirectory&&) = delete;
    TemporaryDirectory& operator=(TemporaryDirectory&&) = delete;

    /** @brief Returns the root directory passed to the controller. */
    const std::filesystem::path& GetPath() const
    {
        return m_path;
    }

private:
    std::filesystem::path m_path;
};

/** @brief Verifies the public response for an existing file. */
void CheckContentType(
        const std::filesystem::path& directory, const std::string_view file_name, const std::string_view expected_type)
{
    const std::string uri = "/" + std::string(file_name);
    CAPTURE(uri, expected_type);
    std::ofstream file(directory / file_name);
    file << "content";
    file.close();
    REQUIRE(file);

    const http_handler::StaticFileController controller(directory.string());
    const http_handler::StringRequest request{http_handler::http::verb::get, uri, 11};
    const auto result = controller.Handle(request, uri);
    const auto* response = std::get_if<http_handler::FileResponse>(&result);
    REQUIRE(response != nullptr);
    CHECK(response->result() == http_handler::http::status::ok);
    CHECK((*response)[http_handler::http::field::content_type] == expected_type);
}
} // namespace

TEST_CASE("Static file responses preserve every supported extension and its uppercase spelling")
{
    const TemporaryDirectory directory;
    const std::pair<std::string_view, std::string_view> content_types[] = {
            {".htm", "text/html"},
            {".html", "text/html"},
            {".css", "text/css"},
            {".txt", "text/plain"},
            {".js", "text/javascript"},
            {".json", "application/json"},
            {".xml", "application/xml"},
            {".png", "image/png"},
            {".jpg", "image/jpeg"},
            {".jpe", "image/jpeg"},
            {".jpeg", "image/jpeg"},
            {".gif", "image/gif"},
            {".bmp", "image/bmp"},
            {".ico", "image/vnd.microsoft.icon"},
            {".tiff", "image/tiff"},
            {".tif", "image/tiff"},
            {".svg", "image/svg+xml"},
            {".svgz", "image/svg+xml"},
            {".mp3", "audio/mpeg"},
    };
    for(const auto& [extension, content_type] : content_types)
    {
        CheckContentType(directory.GetPath(), "asset" + std::string(extension), content_type);
        std::string uppercase_extension(extension);
        std::transform(uppercase_extension.begin(), uppercase_extension.end(), uppercase_extension.begin(),
                [](const unsigned char character)
                {
                    return std::toupper(character);
                });
        CheckContentType(directory.GetPath(), "asset" + uppercase_extension, content_type);
    }
    CheckContentType(directory.GetPath(), "asset.JpEg", "image/jpeg");
}

TEST_CASE("Static file responses use octet-stream for an absent or unknown final extension")
{
    const TemporaryDirectory directory;
    for(const std::string_view file_name : {"asset", "asset.unknown", ".html", "asset.html.unknown", "asset."})
    {
        CheckContentType(directory.GetPath(), file_name, "application/octet-stream");
    }
}
