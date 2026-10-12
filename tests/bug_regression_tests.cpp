#include "application/app.hpp"
#include "controller/request_handler.hpp"
#include "infrastructure/config/json_loader.hpp"
#include "support/memory_repository.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <limits>

namespace {
struct Fixture {
    json_loader::JsonLoader loader{TEST_CONFIG};
    model::Game game = loader.LoadGame();
    app::Players players;
    MemoryRepository repository;
    app::Application application{game, players, app::ApplicationMode::Manual, false, {}, repository};
    view::JsonView view{loader.GetConfiguration()};
    app::JoinGameResult Join(const std::string& name) {
        return application.JoinGame(model::Map::Id{"square"}, name);
    }
};
struct TemporaryDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
                                 ("game-regression-" + domain::RetiredPlayerId::New().ToString());
    TemporaryDirectory() {
        std::filesystem::create_directory(path);
    }
    ~TemporaryDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};
} // namespace

TEST_CASE("Retirement removes exactly the idle subset for every index arrangement") {
    for (unsigned mask = 0; mask < 16; ++mask) {
        CAPTURE(mask);
        Fixture f;
        std::vector<app::JoinGameResult> joined;
        for (unsigned i = 0; i < 4; ++i) {
            joined.push_back(f.Join("player-" + std::to_string(i)));
            if (!(mask & (1u << i)))
                f.application.DoAction(joined.back().token, "R");
        }
        f.application.ManualTick(3000);
        size_t retired = 0;
        for (unsigned i = 0; i < 4; ++i) {
            bool idle = mask & (1u << i);
            REQUIRE(bool(f.players.FindByToken(joined[i].token)) == !idle);
            const auto records = f.repository.records;
            const auto found = std::find_if(records.begin(), records.end(), [&](const auto& record) {
                return record.name == "player-" + std::to_string(i);
            });
            REQUIRE((found != records.end()) == idle);
            retired += idle;
        }
        REQUIRE(f.repository.records.size() == retired);
        auto next = f.Join("newcomer");
        REQUIRE(*next.player_id == 4);
    }
}

TEST_CASE("Session loot generation uses seconds and accumulates small ticks") {
    Fixture f;
    f.game.SetLootGeneratorConfig({1.0, 0.5});
    auto joined = f.Join("Courier");
    for (int i = 0; i < 4; ++i) {
        f.application.ManualTick(200);
        REQUIRE(f.application.GetGameState(joined.token).lost_objects.empty());
    }
    f.application.ManualTick(200);
    REQUIRE(f.application.GetGameState(joined.token).lost_objects.size() == 1);
}

TEST_CASE("Invalid ticks cannot reverse time or overflow player timers") {
    Fixture f;
    auto joined = f.Join("Courier");
    http_handler::ApiController controller(f.application, f.view);
    auto before = f.view.Render(f.application.GetGameState(joined.token));
    for (const auto& body :
         {"{\"timeDelta\":-1}", "{\"timeDelta\":4294967296}", "{\"timeDelta\":2147483648}"}) {
        http_handler::StringRequest request{boost::beast::http::verb::post, "/api/v1/game/tick", 11};
        request.set(boost::beast::http::field::content_type, "application/json");
        request.body() = body;
        REQUIRE(controller.Handle(request, request.target()).result_int() == 400);
        REQUIRE(f.view.Render(f.application.GetGameState(joined.token)) == before);
    }
    REQUIRE_THROWS(f.application.ManualTick(-1));
    model::Dog dog{model::Dog::Id{0}, "long-running", {0, 0}, 1.0, "U", 3, 0};
    dog.UpdateState(std::numeric_limits<int>::max());
    REQUIRE_THROWS(dog.UpdateState(1));
    REQUIRE(dog.GetPlayTime() == std::numeric_limits<int>::max());
}

TEST_CASE("Negative pagination is rejected before reaching PostgreSQL") {
    Fixture f;
    http_handler::ApiController controller(f.application, f.view);
    for (const auto* uri : {"/api/v1/game/records?start=-1", "/api/v1/game/records?maxItems=-1"}) {
        http_handler::StringRequest request{boost::beast::http::verb::get, uri, 11};
        REQUIRE(controller.Handle(request, uri).result_int() == 400);
    }
}

TEST_CASE("Malformed percent escapes and encoded NUL return a client error") {
    Fixture f;
    boost::asio::io_context context;
    auto handler = std::make_shared<http_handler::RequestHandler>(".", boost::asio::make_strand(context),
                                                                  f.application, f.view);
    for (const auto* uri : {"/api/v1/maps/%", "/api/v1/maps/%0", "/api/v1/maps/%GG", "/api/v1/maps/%00"}) {
        http_handler::StringRequest request{boost::beast::http::verb::get, uri, 11};
        int status = 0;
        (*handler)(std::move(request), [&](auto&& response) { status = response.result_int(); });
        context.run();
        context.restart();
        REQUIRE(status == 400);
    }
}

TEST_CASE("Directory index symlinks cannot escape the static root") {
    TemporaryDirectory dir;
    auto root = dir.path / "public";
    std::filesystem::create_directory(root);
    auto private_file = dir.path / "private.txt";
    std::ofstream(private_file) << "must not be served";
    std::filesystem::create_symlink(private_file, root / "index.html");
    http_handler::StaticFileController controller(root.string());
    http_handler::StringRequest request{boost::beast::http::verb::get, "/", 11};
    auto result = controller.Handle(request, "/");
    REQUIRE(std::visit([](const auto& response) { return response.result_int(); }, result) == 400);
}

TEST_CASE("Integral JSON numbers load as numeric settings and empty roads fail cleanly") {
    TemporaryDirectory dir;
    Fixture f;
    auto config = f.loader.GetConfiguration();
    auto& object = config.as_object();
    object["defaultDogSpeed"] = 3;
    object["dogRetirementTime"] = 60;
    object["lootGeneratorConfig"] = boost::json::object{{"period", 1}, {"probability", 0}};
    auto path = dir.path / "config.json";
    std::ofstream(path) << boost::json::serialize(config);
    json_loader::JsonLoader loader(path);
    auto game = loader.LoadGame();
    REQUIRE(game.FindMap(model::Map::Id{"square"})->GetSpeed() == 3.0);
    REQUIRE(game.GetDogRetirementTime() == 60000);
    object.at("maps").as_array()[0].as_object()["roads"] = boost::json::array{};
    std::ofstream(path) << boost::json::serialize(config);
    json_loader::JsonLoader invalid(path);
    REQUIRE_THROWS(invalid.LoadGame());
}

TEST_CASE("Fractional base speed is not truncated by the model accessor") {
    model::Dog dog{model::Dog::Id{0}, "Courier", {0, 0}, 2.75, "U", 3, 0};
    REQUIRE(dog.GetAbsSpeed() == 2.75);
}

TEST_CASE("Records route does not accept arbitrary suffixes") {
    Fixture f;
    http_handler::ApiController controller(f.application, f.view);
    for (const auto* uri : {"/api/v1/game/recordsmissing", "/api/v1/game/records.txt"}) {
        http_handler::StringRequest request{boost::beast::http::verb::get, uri, 11};
        REQUIRE(controller.Handle(request, uri).result_int() == 400);
    }
}
