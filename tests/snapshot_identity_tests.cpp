#include <filesystem>
#include <fstream>
#include <limits>
#include <unordered_map>

#include <catch2/catch_test_macros.hpp>
#include <boost/archive/text_oarchive.hpp>

#include "application/app.hpp"
#include "infrastructure/persistence/model_serialization.hpp"
#include "infrastructure/persistence/state_store.hpp"
#include "support/memory_repository.h"

namespace
{
/** @brief Isolated snapshot file removed after each test. */
class TemporarySnapshot
{
public:
    /** @brief Allocates a unique path without creating a snapshot. */
    TemporarySnapshot():
            m_path(std::filesystem::temp_directory_path() /
                    ("item-identities-" + domain::RetiredPlayerId::New().ToString()))
    {
    }

    /** @brief Removes the saved file without throwing during cleanup. */
    ~TemporarySnapshot()
    {
        std::error_code error;
        std::filesystem::remove(m_path, error);
        std::filesystem::remove(m_path.string() + ".tmp", error);
    }

    /** @brief Returns the path used by the real snapshot store. */
    const std::filesystem::path& GetPath() const
    {
        return m_path;
    }

private:
    /** @brief Snapshot path owned by this test fixture. */
    std::filesystem::path m_path;
};

/** @brief Creates a straight road with deterministic item generation settings. */
model::Game MakeGame(const double loot_probability)
{
    model::Map map(model::Map::Id{"road"}, "Road");
    map.AddRoad({model::Road::HORIZONTAL, {0, 0}, 100});
    map.SetSpeed(2.0);
    map.SetBagCapacity(3);
    map.AddLoot(0, {0, 10});
    model::Game game;
    game.AddMap(map);
    game.SetLootGeneratorConfig({1.0, loot_probability});
    game.SetDogRetirementTime(60000);
    return game;
}

/** @brief Simulates a committed write whose acknowledgement was lost. */
class UncertainCommitRepository : public domain::RetiredPlayerRepository
{
public:
    /** @brief Records each attempted identity and fails after the first result is stored. */
    void SaveRetiredPlayer(const domain::RetiredPlayer& player) override
    {
        m_attempts.push_back(player.id);
        m_records.emplace(player.id, player);
        if (m_attempts.size() == 1)
        {
            throw std::runtime_error("Commit acknowledgement lost");
        }
    }

    /** @brief Returns stored results for assertions independent of SQL. */
    std::vector<domain::RetiredPlayer> GetRetiredPlayers(int, int) override
    {
        std::vector<domain::RetiredPlayer> records;
        for(const auto& [id, player] : m_records)
        {
            records.push_back(player);
        }
        return records;
    }

    /** @brief Returns the identities passed by the application to the repository. */
    const std::vector<std::string>& GetAttempts() const
    {
        return m_attempts;
    }

private:
    /** @brief Saved rows keyed by finished-game identity, matching a database primary key. */
    std::unordered_map<std::string, domain::RetiredPlayer> m_records;
    /** @brief Application write attempts, including the failed acknowledgement. */
    std::vector<std::string> m_attempts;
};

/** @brief Writes the version-zero player fields to exercise archive compatibility. */
struct PlayerSnapshotV0
{
    /** @brief Copies the fields defined by archive version zero. */
    explicit PlayerSnapshotV0(const app::Player& player):
            id(*player.GetId()),
            token(*player.GetToken()),
            map_id(*player.GetSession()->GetMap()->GetId()),
            dog_id(*player.GetDog()->GetId())
    {
    }

    /** @brief Writes the original version-zero field order. */
    template <typename Archive> void serialize(Archive& archive, unsigned)
    {
        archive & id & token & map_id & dog_id;
    }

    std::uint32_t id;
    std::string token;
    std::string map_id;
    std::uint32_t dog_id;
};

/** @brief Writes the registry envelope containing version-zero player entries. */
struct PlayersSnapshotV0
{
    /** @brief Writes the registry vector using its archived element version. */
    template <typename Archive> void serialize(Archive& archive, unsigned)
    {
        archive & players;
    }

    std::vector<PlayerSnapshotV0> players;
};
} // namespace

TEST_CASE("Restart never gives a new item the identity of an item still in a bag")
{
    auto game = MakeGame(0.0);
    app::Players players;
    MemoryRepository repository;
    app::Application application(game, players, app::ApplicationMode::Manual, false, {}, repository);
    const auto joined = application.JoinGame(model::Map::Id{"road"}, "Courier");
    auto* session = game.FindSession(model::Map::Id{"road"});
    session->AddLoot({model::Loot::Id{0}, {0, 10}, {1, 0}});
    application.DoAction(joined.token, "R");
    application.ManualTick(500);
    REQUIRE(application.GetGameState(joined.token).players.front().bag.size() == 1);
    REQUIRE(session->GetLoots().empty());

    TemporarySnapshot snapshot;
    persistence::StateStore store(snapshot.GetPath(), {});
    store.Save(game, players);
    auto restored_game = MakeGame(1.0);
    app::Players restored_players;
    store.Restore(restored_game, restored_players);
    app::Application restored(restored_game, restored_players, app::ApplicationMode::Manual, false, {}, repository);
    restored.ManualTick(1000);

    const auto state = restored.GetGameState(joined.token);
    REQUIRE(state.players.front().bag.size() == 1);
    REQUIRE(state.lost_objects.size() == 1);
    REQUIRE(state.lost_objects.front().GetId() != state.players.front().bag.front().GetId());
}

TEST_CASE("Registering a dog reserves every live item identity in its bag")
{
    auto game = MakeGame(0.0);
    model::GameSession session(model::GameSession::Id{0}, game.FindMap(model::Map::Id{"road"}), {}, 60000);
    model::Dog dog(model::Dog::Id{0}, "Courier", {0, 0}, 2.0, "U", 3, 0);
    dog.PutLootInBag({model::Loot::Id{20}, {0, 10}, {0, 0}});
    dog.PutLootInBag({model::Loot::Id{2}, {0, 10}, {0, 0}});
    session.AddDog(dog);
    session.AddLoot({model::Loot::Id{5}, {0, 10}, {10, 0}});
    REQUIRE(session.GetNextLootId() == 21);

    model::Dog invalid(model::Dog::Id{1}, "Invalid", {0, 0}, 2.0, "U", 1, 0);
    invalid.PutLootInBag({model::Loot::Id{std::numeric_limits<unsigned>::max()}, {0, 10}, {0, 0}});
    REQUIRE_THROWS_AS(session.AddDog(invalid), std::invalid_argument);
    REQUIRE(session.GetDogs().size() == 1);
    REQUIRE(session.GetNextDogId() == 1);
    REQUIRE(session.GetNextLootId() == 21);
}

TEST_CASE("Retirement keeps one result identity after an uncertain commit and snapshot restart")
{
    auto game = MakeGame(0.0);
    app::Players players;
    UncertainCommitRepository repository;
    app::Application application(game, players, app::ApplicationMode::Manual, false, {}, repository);
    const auto joined = application.JoinGame(model::Map::Id{"road"}, "Courier");
    TemporarySnapshot snapshot;
    persistence::StateStore store(snapshot.GetPath(), {});
    store.Save(game, players);
    REQUIRE_THROWS_AS(application.ManualTick(60000), std::runtime_error);
    REQUIRE(repository.GetAttempts().size() == 1);
    REQUIRE(players.FindByToken(joined.token));

    SECTION("Retry in the current process")
    {
        application.ManualTick(0);
        REQUIRE_FALSE(players.FindByToken(joined.token));
    }
    SECTION("Retry from the last active snapshot")
    {
        auto restored_game = MakeGame(0.0);
        app::Players restored_players;
        store.Restore(restored_game, restored_players);
        app::Application restored(
                restored_game, restored_players, app::ApplicationMode::Manual, false, {}, repository);
        restored.ManualTick(60000);
        REQUIRE_FALSE(restored_players.FindByToken(joined.token));
    }
    REQUIRE(repository.GetAttempts().size() == 2);
    REQUIRE(repository.GetAttempts().front() == repository.GetAttempts().back());
    REQUIRE(repository.GetRetiredPlayers(0, 100).size() == 1);
}

TEST_CASE("Finished-game identities are nonnil and unique among registered players")
{
    auto game = MakeGame(0.0);
    app::Players players;
    MemoryRepository repository;
    app::Application application(game, players, app::ApplicationMode::Manual, false, {}, repository);
    const auto joined = application.JoinGame(model::Map::Id{"road"}, "Courier");
    const auto player = players.FindByToken(joined.token);
    auto* session = player->GetSession();
    session->AddDog({model::Dog::Id{1}, "Other", {0, 0}, 2.0, "U", 1, 0});
    const auto other_dog = session->FindDog(model::Dog::Id{1});

    REQUIRE_THROWS_AS((app::Player{app::Player::Id{1}, app::Token{"other"}, session, other_dog,
                              domain::RetiredPlayerId{}}), std::invalid_argument);
    REQUIRE_THROWS_AS(players.AddPlayer({app::Player::Id{1}, app::Token{"other"}, session, other_dog,
                             player->GetResultId()}), std::invalid_argument);
    REQUIRE(players.GetPlayersList().size() == 1);
    REQUIRE(players.GetNextPlayerId() == 1);
}

TEST_CASE("Version-zero snapshots restore players and persist their allocated result identity")
{
    auto game = MakeGame(0.0);
    app::Players players;
    MemoryRepository repository;
    app::Application application(game, players, app::ApplicationMode::Manual, false, {}, repository);
    const auto joined = application.JoinGame(model::Map::Id{"road"}, "Courier");
    TemporarySnapshot snapshot;
    {
        std::ofstream output(snapshot.GetPath());
        boost::archive::text_oarchive archive(output);
        std::vector<serialization::GameSessionRepr> sessions;
        sessions.emplace_back(game.GetSessions().front());
        PlayersSnapshotV0 registry;
        registry.players.emplace_back(*players.FindByToken(joined.token));
        archive << sessions << registry;
    }
    auto restored_game = MakeGame(0.0);
    app::Players restored_players;
    persistence::StateStore store(snapshot.GetPath(), {});
    store.Restore(restored_game, restored_players);
    const auto restored_player = restored_players.FindByToken(joined.token);
    REQUIRE(restored_player);
    const auto result_id = restored_player->GetResultId();
    REQUIRE(result_id != domain::RetiredPlayerId{});
    store.Save(restored_game, restored_players);

    auto next_game = MakeGame(0.0);
    app::Players next_players;
    store.Restore(next_game, next_players);
    REQUIRE(next_players.FindByToken(joined.token)->GetResultId() == result_id);
}

TEST_CASE("Snapshot result identities reject malformed nil and duplicated values atomically")
{
    auto game = MakeGame(0.0);
    app::Players players;
    MemoryRepository repository;
    app::Application application(game, players, app::ApplicationMode::Manual, false, {}, repository);
    const auto first = application.JoinGame(model::Map::Id{"road"}, "First");
    const auto second = application.JoinGame(model::Map::Id{"road"}, "Second");
    const auto first_id = players.FindByToken(first.token)->GetResultId().ToString();
    const auto second_id = players.FindByToken(second.token)->GetResultId().ToString();
    std::string replacement;
    SECTION("Malformed UUID")
    {
        replacement = std::string(second_id.size(), 'z');
    }
    SECTION("Nil UUID")
    {
        replacement = domain::RetiredPlayerId{}.ToString();
    }
    SECTION("Duplicated result UUID")
    {
        replacement = first_id;
    }
    TemporarySnapshot snapshot;
    persistence::StateStore store(snapshot.GetPath(), {});
    store.Save(game, players);
    std::ifstream input(snapshot.GetPath());
    std::string contents{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const auto offset = contents.find(second_id);
    REQUIRE(offset != std::string::npos);
    contents.replace(offset, second_id.size(), replacement);
    std::ofstream(snapshot.GetPath()) << contents;

    auto restored_game = MakeGame(0.0);
    app::Players restored_players;
    REQUIRE_THROWS(store.Restore(restored_game, restored_players));
    REQUIRE(restored_game.GetSessions().empty());
    REQUIRE(restored_players.GetPlayersList().empty());
}
