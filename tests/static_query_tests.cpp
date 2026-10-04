#include "controller/request_handler.h"
#include "infrastructure/config/json_loader.h"
#include "support/memory_repository.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

TEST_CASE("Static requests ignore queries without losing encoded filename characters") {
    const auto directory = std::filesystem::temp_directory_path() /
                           ("static-query-" + domain::RetiredPlayerId::New().ToString());
    std::filesystem::create_directory(directory);
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    } cleanup{directory};
    std::ofstream(directory / "index.html") << "home";
    std::ofstream(directory / "script.js") << "var ready = true;";
    std::ofstream(directory / "question?.txt") << "question";
    json_loader::JsonLoader loader(TEST_CONFIG);
    auto game = loader.LoadGame();
    app::Players players;
    MemoryRepository repository;
    app::Application application(game, players, app::ApplicationMode::Manual, false, {}, repository);
    view::JsonView view(*loader.GetConfig());
    boost::asio::io_context context;
    auto handler = std::make_shared<http_handler::RequestHandler>(
        directory.string(), boost::asio::make_strand(context), application, view);
    for (const char* uri :
         {"/?", "/?map=town", "/index.html?v=1", "/script.js?v=123", "/question%3F.txt?cache=1"}) {
        CAPTURE(uri);
        http_handler::StringRequest request{boost::beast::http::verb::get, uri, 11};
        int status = 0;
        (*handler)(std::move(request), [&](auto&& response) { status = response.result_int(); });
        context.run();
        context.restart();
        REQUIRE(status == 200);
    }
}
