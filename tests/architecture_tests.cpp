#include "application/app.hpp"
#include "controller/request_handler.hpp"
#include "infrastructure/config/json_loader.hpp"
#include "infrastructure/http/run_workers.hpp"
#include "infrastructure/persistence/app_serialization.hpp"
#include "infrastructure/persistence/model_serialization.hpp"
#include "infrastructure/persistence/state_store.hpp"
#include "support/memory_repository.h"
#include <boost/archive/text_oarchive.hpp>
#include <boost/asio/post.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <type_traits>

namespace {
struct Fixture {
    json_loader::JsonLoader loader{TEST_CONFIG};
    model::Game game = loader.LoadGame();
    app::Players players;
    MemoryRepository repository;
    app::Application application{game, players, app::ApplicationMode::Manual, false, {}, repository};
    app::JoinGameResult Join() {
        return application.JoinGame(model::Map::Id{"square"}, "Courier");
    }
};
struct Directory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
                                 ("architecture-" + domain::RetiredPlayerId::New().ToString());
    Directory() {
        std::filesystem::create_directory(path);
    }
    ~Directory() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};
model::Dog Dog(unsigned id) {
    return {model::Dog::Id{id}, "Courier", {0, 0}, 2, "U", 3, 0};
}
void WriteSnapshot(const std::filesystem::path& path,
                   const std::vector<serialization::GameSessionRepr>& sessions, const app::Players& players) {
    std::ofstream file(path);
    boost::archive::text_oarchive archive(file);
    serialization::PlayersRepr repr(players);
    archive << sessions << repr;
}
} // namespace

TEST_CASE("Game ownership is move-only and map addresses remain stable") {
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<model::Game>);
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<app::Application>);
    Fixture f;
    auto joined = f.Join();
    auto* map = f.game.FindMap(model::Map::Id{"square"});
    for (int i = 0; i < 200; ++i)
        f.game.AddMap({model::Map::Id{"map-" + std::to_string(i)}, "Extra"});
    REQUIRE(f.game.FindMap(model::Map::Id{"square"}) == map);
    REQUIRE(f.players.FindByToken(joined.token)->GetSession()->GetMap() == map);
    f.application.ManualTick(1);
}

TEST_CASE("Aggregate registries reject duplicates without changing their indexes") {
    Fixture f;
    auto joined = f.Join();
    auto player = f.players.FindByToken(joined.token);
    auto* session = player->GetSession();
    REQUIRE_THROWS_AS(session->AddDog(Dog(0)), std::invalid_argument);
    REQUIRE(session->FindDog(model::Dog::Id{0}) == player->GetDog());
    REQUIRE(session->GetDogs().size() == 1);
    REQUIRE_THROWS_AS(f.players.AddPlayer(*player), std::invalid_argument);
    REQUIRE(f.players.GetPlayersList().size() == 1);
    REQUIRE(f.players.FindByToken(joined.token) == player);
    REQUIRE_THROWS_AS(f.game.AddSession(model::GameSession(session->GetId(), session->GetMap(), {}, 1000)),
                      std::invalid_argument);
    REQUIRE(f.game.GetSessions().size() == 1);
    REQUIRE(f.game.FindSession(session->GetId()) == session);
    session->AddLoot({model::Loot::Id{7}, {0, 10}, {0, 0}});
    REQUIRE_THROWS_AS(session->AddLoot({model::Loot::Id{7}, {0, 10}, {1, 0}}), std::invalid_argument);
    REQUIRE(session->GetLoots().size() == 1);
}

TEST_CASE("Restored identities allocate from the maximum instead of insertion order") {
    Fixture f;
    const auto* map = f.game.FindMap(model::Map::Id{"square"});
    f.game.AddSession(model::GameSession{model::GameSession::Id{20}, map, {}, 3000});
    const auto* other = f.game.FindMap(model::Map::Id{"other"});
    f.game.AddSession(model::GameSession{model::GameSession::Id{2}, other, {}, 3000});
    REQUIRE(f.game.GetNextSessionId() == 21);
    auto* session = f.game.FindSession(map->GetId());
    session->AddDog(Dog(20));
    session->AddDog(Dog(2));
    REQUIRE(session->GetNextDogId() == 21);
    session->AddLoot({model::Loot::Id{20}, {0, 10}, {0, 0}});
    session->AddLoot({model::Loot::Id{2}, {0, 10}, {0, 0}});
    REQUIRE(session->GetNextLootId() == 21);
    f.players.AddPlayer({app::Player::Id{20}, app::Token{"one"}, session, session->FindDog(model::Dog::Id{20})});
    f.players.AddPlayer({app::Player::Id{2}, app::Token{"two"}, session, session->FindDog(model::Dog::Id{2})});
    REQUIRE(f.players.GetNextPlayerId() == 21);
}

TEST_CASE("Retirement callback does not retain a destroyed application") {
    json_loader::JsonLoader loader(TEST_CONFIG);
    auto game = loader.LoadGame();
    app::Players players;
    MemoryRepository repository;
    {
        app::Application application(game, players, app::ApplicationMode::Manual, false, {}, repository);
        application.JoinGame(model::Map::Id{"square"}, "Courier");
    }
    game.UpdateState(3000);
    REQUIRE(repository.records.size() == 1);
    REQUIRE(players.GetPlayersList().empty());
}

TEST_CASE("Repository failures remain server errors and do not disclose internal details") {
    struct FailingRepository : domain::RetiredPlayerRepository {
        void SaveRetiredPlayer(const domain::RetiredPlayer&) override {
            throw std::invalid_argument("private-database-details");
        }
        std::vector<domain::RetiredPlayer> GetRetiredPlayers(int, int) override {
            return {};
        }
    } repository;
    Fixture f;
    app::Application application(f.game, f.players, app::ApplicationMode::Manual, false, {}, repository);
    auto joined = application.JoinGame(model::Map::Id{"square"}, "Courier");
    boost::asio::io_context context;
    view::JsonView view(f.loader.GetConfiguration());
    std::string diagnostic;
    auto handler = std::make_shared<http_handler::RequestHandler>(
        ".", boost::asio::make_strand(context), application, view,
        [&](std::string_view message) { diagnostic = message; });
    http_handler::StringRequest request{boost::beast::http::verb::post, "/api/v1/game/tick", 11};
    request.set(boost::beast::http::field::content_type, "application/json");
    request.body() = R"({"timeDelta":3000})";
    int status = 0;
    std::string body;
    (*handler)(std::move(request), [&](auto&& response) {
        status = response.result_int();
        if constexpr (std::is_same_v<std::decay_t<decltype(response)>, http_handler::StringResponse>)
            body = response.body();
    });
    context.run();
    REQUIRE(status == 500);
    REQUIRE(body == "Internal server error");
    REQUIRE(diagnostic == "private-database-details");
    REQUIRE(f.players.FindByToken(joined.token));
}

TEST_CASE("Worker failure stops the context and reaches the caller after threads join") {
    for (unsigned workers : {1u, 4u}) {
        boost::asio::io_context context;
        auto keep_alive = boost::asio::make_work_guard(context);
        boost::asio::post(context, [] { throw std::runtime_error("background failure"); });
        REQUIRE_THROWS_AS(http_server::RunWorkers(context, workers), std::runtime_error);
        REQUIRE(context.stopped());
    }
}

TEST_CASE("Invalid snapshot references leave the destination untouched") {
    Fixture source;
    auto joined = source.Join();
    Directory directory;
    auto path = directory.path / "state";
    persistence::StateStore store(path, {});
    SECTION("Missing session") {
        WriteSnapshot(path, {}, source.players);
    }
    SECTION("Missing dog") {
        auto* session = source.players.FindByToken(joined.token)->GetSession();
        model::GameSession empty(session->GetId(), session->GetMap(), {}, 3000);
        WriteSnapshot(path, {serialization::GameSessionRepr(empty)}, source.players);
    }
    SECTION("Missing map after a valid session") {
        model::Map alien(model::Map::Id{"alien"}, "Alien");
        alien.AddRoad({model::Road::HORIZONTAL, {0, 0}, 10});
        model::GameSession invalid(model::GameSession::Id{30}, &alien, {}, 3000);
        WriteSnapshot(path,
                      {serialization::GameSessionRepr(source.game.GetSessions().front()),
                       serialization::GameSessionRepr(invalid)},
                      source.players);
    }
    SECTION("Position outside roads") {
        source.players.FindByToken(joined.token)->GetDog()->SetPosition({1000, 1000});
        store.Save(source.game, source.players);
    }
    auto destination = source.loader.LoadGame();
    app::Players players;
    const auto* old_map = destination.FindMap(model::Map::Id{"square"});
    REQUIRE_THROWS(store.Restore(destination, players));
    REQUIRE(destination.GetSessions().empty());
    REQUIRE(players.GetPlayersList().empty());
    REQUIRE(destination.FindMap(model::Map::Id{"square"}) == old_map);
}

TEST_CASE("Failed periodic save retries without resetting its elapsed threshold") {
    Fixture f;
    f.Join();
    Directory directory;
    auto parent = directory.path / "missing";
    auto path = parent / "state";
    persistence::StateStore store(path, 100);
    REQUIRE_THROWS(store.OnTick(101, f.game, f.players));
    std::filesystem::create_directory(parent);
    store.OnTick(0, f.game, f.players);
    REQUIRE(std::filesystem::exists(path));
}
