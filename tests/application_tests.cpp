#include "application/app.h"
#include "infrastructure/config/json_loader.h"
#include "infrastructure/persistence/state_store.h"
#include "support/memory_repository.h"
#include "view/json_view.h"
#include <boost/json.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

namespace {
struct Fixture {
    json_loader::JsonLoader loader{TEST_CONFIG};
    model::Game game = loader.LoadGame();
    app::Players players;
    MemoryRepository repository;
    app::Application application{game, players, app::ApplicationMode::Manual, false, {}, repository};
    view::JsonView view{*loader.GetConfig()};

    app::JoinGameResult Join(const char* name, const char* map = "square") {
        return application.JoinGame(model::Map::Id{map}, name);
    }
};
struct TemporaryState {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("game-state-" + domain::RetiredPlayerId::New().ToString());
    ~TemporaryState() {
        std::error_code error;
        std::filesystem::remove(path, error);
        std::filesystem::remove(path.string() + ".tmp", error);
    }
};
} // namespace

TEST_CASE("Players see only their own session and retain movement semantics") {
    Fixture f;
    auto alice = f.Join("Alice");
    f.Join("Bob");
    auto other = f.Join("Other", "other");
    REQUIRE(f.application.GetPlayersList(alice.token).players.size() == 2);
    REQUIRE(f.application.GetPlayersList(other.token).players.size() == 1);
    f.application.DoAction(alice.token, "R");
    f.application.ManualTick(1000);
    auto state = f.application.GetGameState(alice.token);
    REQUIRE(state.players.front().position.x == 2.0);
    REQUIRE(state.players.front().position.y == 0.0);
    REQUIRE(state.players.front().speed.x == 2.0);
    f.application.DoAction(alice.token, "");
    f.application.ManualTick(500);
    REQUIRE(f.application.GetGameState(alice.token).players.front().position.x == 2.0);
}

TEST_CASE("Retirement persists the record and invalidates authorization") {
    Fixture f;
    auto player = f.Join("Courier");
    f.application.ManualTick(3000);
    REQUIRE(f.repository.records.size() == 1);
    REQUIRE(f.repository.records[0].name == "Courier");
    REQUIRE(f.repository.records[0].play_time_ms == 3000);
    REQUIRE_THROWS_AS(f.application.GetGameState(player.token), app::ApplicationError);
    REQUIRE(f.players.GetPlayersList().empty());
    const auto result = f.view.Render(f.application.GetGameRecords({}, {}));
    REQUIRE(result[0].as_object().at("playTime").as_double() == 3.0);
}

TEST_CASE("JSON view preserves map metadata and hides runtime settings") {
    Fixture f;
    auto description = f.view.Render(f.application.GetMapDesc(model::Map::Id{"square"}));
    REQUIRE(description.size() == 6);
    REQUIRE(description.at("lootTypes").as_array()[0].as_object().at("color") == "#fa0");
    REQUIRE_FALSE(description.contains("dogSpeed"));
    auto player = f.Join("Alice");
    auto json = f.view.Render(f.application.GetGameState(player.token));
    REQUIRE(json.at("players").as_object().at("0").as_object().at("bag").as_array().empty());
    REQUIRE(json.at("lostObjects").as_object().empty());
}

TEST_CASE("Snapshot round trip preserves tokens, bags, scores and session bindings") {
    Fixture f;
    auto player = f.Join("Saved");
    auto dog = f.players.FindByToken(player.token)->GetDog();
    dog->PutLootInBag(model::Loot{model::Loot::Id{20}, {0, 10}, {0, 0}});
    dog->ExchangeLoot();
    dog->PutLootInBag(model::Loot{model::Loot::Id{21}, {0, 10}, {0, 0}});
    f.application.DoAction(player.token, "R");
    f.application.ManualTick(500);
    TemporaryState file;
    persistence::StateStore store{file.path, {}};
    store.Save(f.game, f.players);
    REQUIRE(std::filesystem::exists(file.path));
    REQUIRE_FALSE(std::filesystem::exists(file.path.string() + ".tmp"));
    auto restored_game = f.loader.LoadGame();
    app::Players restored_players;
    store.Restore(restored_game, restored_players);
    app::Application restored{restored_game, restored_players, app::ApplicationMode::Manual, false, {},
                              f.repository};
    auto expected = f.view.Render(f.application.GetGameState(player.token));
    // The original snapshot format does not retain movement velocity.
    expected.at("players").as_object().at("0").as_object().at("speed") = boost::json::array{0.0, 0.0};
    REQUIRE(f.view.Render(restored.GetGameState(player.token)) == expected);
    REQUIRE(restored_players.FindByToken(player.token)->GetSession() ==
            restored_game.FindSession(model::Map::Id{"square"}));
    auto next = restored.JoinGame(model::Map::Id{"square"}, "Next");
    REQUIRE(*next.player_id > *player.player_id);
    restored.DoAction(player.token, "");
    restored.ManualTick(3000);
    REQUIRE_THROWS_AS(restored.GetGameState(player.token), app::ApplicationError);
}

TEST_CASE("Snapshot timer keeps the strict original threshold") {
    Fixture f;
    TemporaryState file;
    persistence::StateStore store{file.path, 100};
    store.OnTick(100, f.game, f.players);
    REQUIRE_FALSE(std::filesystem::exists(file.path));
    store.OnTick(1, f.game, f.players);
    REQUIRE(std::filesystem::exists(file.path));
}

TEST_CASE("Tick modes reject the other clock and notify after updates") {
    Fixture f;
    int elapsed = 0;
    app::Application automatic{
        f.game,      f.players, app::ApplicationMode::Auto, false, [&](int delta) { elapsed += delta; },
        f.repository};
    REQUIRE_THROWS_AS(automatic.ManualTick(20), app::ApplicationError);
    automatic.AutoTick(20);
    REQUIRE(elapsed == 20);
    REQUIRE_THROWS_AS(f.application.AutoTick(20), app::ApplicationError);
}
