#pragma once
#include "controller/http_types.h"
namespace http_handler {
class StaticFileController {
  public:
    explicit StaticFileController(std::string_view root) : static_path_(root) {}
    FileRequestResult Handle(const StringRequest& req, std::string_view uri);

  private:
    static std::string_view GetFileContentType(const fs::path& path);
    static bool IsSubPath(fs::path path, fs::path base);
    fs::path static_path_;
};
} // namespace http_handler
