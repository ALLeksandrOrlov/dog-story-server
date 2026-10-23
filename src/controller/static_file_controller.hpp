#pragma once

#include "controller/http_types.hpp"

namespace http_handler
{
/** @brief Serves files from one directory, checking path traversal and symbolic links. */
class StaticFileController
{
public:
    /** @brief Stores the root directory; resolves paths when handling requests. */
    explicit StaticFileController(const std::string_view root):
            m_static_path(root)
    {
    }
    /**
     * @brief Opens a file or a directory's index.html and determines its Content-Type.
     * @param request HTTP request; method semantics follow the static server contract.
     * @param uri Decoded path without the query string.
     * @return File response or a 400/404 error; the returned response owns the file stream.
     * @throws std::filesystem::filesystem_error A filesystem access error occurred.
     * @throws std::runtime_error The file exists but could not be opened.
     */
    FileRequestResult Handle(const StringRequest& request, const std::string_view uri) const;

private:
    /** @brief Root directory that resolved paths must not escape. */
    const fs::path m_static_path;
    /** @brief Maps extensions to MIME types; unknown extensions use application/octet-stream. */
    static std::string_view GetFileContentType(const fs::path& path);
    /** @brief Compares canonical path components rather than string prefixes. */
    static bool IsSubPath(const fs::path& path, const fs::path& base);
};
} // namespace http_handler
