#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "application/app.hpp"
#include "domain/collision_detector.hpp"

namespace
{

class ReviewRepository final : public domain::RetiredPlayerRepository
{
public:
    void SaveRetiredPlayer(const domain::RetiredPlayer& player) override
    {
        records.emplace_back(player);
    }
    std::vector<domain::RetiredPlayer> GetRetiredPlayers(const int, const int) override
    {
        return records;
    }
    std::vector<domain::RetiredPlayer> records{};
};

model::Game MakeReviewGame()
{
    model::Map map(model::Map::Id{"review"}, "Review map");
    map.AddRoad({model::Road::HORIZONTAL, {0, 0}, 10});
    model::Game game;
    game.AddMap(map);
    return game;
}

model::Dog MakeReviewDog(const unsigned id = 0, const int score = 0)
{
    return {model::Dog::Id{id}, "Courier", {0, 0}, 1.0, "U", 3, score};
}
} // namespace

TEST_CASE("Aggregate registries cannot be copied or structurally modified from getters")
{
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<model::GameSession>);
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<app::Players>);
    STATIC_REQUIRE(std::is_move_constructible_v<model::GameSession>);
    STATIC_REQUIRE(std::is_move_constructible_v<app::Players>);
    STATIC_REQUIRE(std::is_const_v<std::remove_reference_t<decltype(std::declval<model::Game&>().GetSessions())>>);
    STATIC_REQUIRE(std::has_virtual_destructor_v<domain::RetiredPlayerRepository>);
    STATIC_REQUIRE(std::has_virtual_destructor_v<collision_detector::ItemGathererProvider>);
}

TEST_CASE("Token failures and collisions do not leave unregistered dogs")
{
    auto game = MakeReviewGame();
    app::Players players;
    ReviewRepository repository;
    const app::Token first_token{std::string(32, 'a')};
    const app::Token second_token{std::string(32, 'b')};
    int generation = 0;
    app::Application application(game, players, app::ApplicationMode::Manual, false, {}, repository,
        [&]() { return (++generation <= 2) ? first_token : second_token; });
    const auto first = application.JoinGame(model::Map::Id{"review"}, "First");
    const auto second = application.JoinGame(model::Map::Id{"review"}, "Second");
    REQUIRE(first.token == first_token);
    REQUIRE(second.token == second_token);
    REQUIRE(generation == 3);
    REQUIRE(game.GetSessions().front().GetDogs().size() == 2);

    app::Application broken(game, players, app::ApplicationMode::Manual, false, {}, repository,
        [&]() { return first_token; });
    REQUIRE_THROWS_AS(broken.JoinGame(model::Map::Id{"review"}, "Rejected"), std::runtime_error);
    REQUIRE(players.GetPlayersList().size() == 2);
    REQUIRE(game.GetSessions().front().GetDogs().size() == 2);
}

TEST_CASE("Unavailable or malformed token source leaves an empty world unchanged")
{
    auto game = MakeReviewGame();
    app::Players players;
    ReviewRepository repository;
    SECTION("source throws")
    {
        app::Application application(game, players, app::ApplicationMode::Manual, false, {}, repository,
            []() -> app::Token { throw std::runtime_error("No entropy"); });
        REQUIRE_THROWS_AS(application.JoinGame(model::Map::Id{"review"}, "Courier"), std::runtime_error);
    }
    SECTION("source returns malformed value")
    {
        app::Application application(game, players, app::ApplicationMode::Manual, false, {}, repository,
            []() { return app::Token{"invalid"}; });
        REQUIRE_THROWS_AS(application.JoinGame(model::Map::Id{"review"}, "Courier"), std::runtime_error);
    }
    REQUIRE(game.GetSessions().empty());
    REQUIRE(players.GetPlayersList().empty());
}

TEST_CASE("Exhausted player identities reject join before adding a dog")
{
    auto game = MakeReviewGame();
    app::Players players;
    ReviewRepository repository;
    const auto* map = game.FindMap(model::Map::Id{"review"});
    game.AddSession(model::GameSession(model::GameSession::Id{0}, map, {}, 60000));
    auto* session = game.FindSession(map->GetId());
    session->AddDog(MakeReviewDog());
    const auto maximum_player = static_cast<unsigned>(std::numeric_limits<int>::max() - 1);
    players.AddPlayer({app::Player::Id{maximum_player}, app::Token{std::string(32, 'a')}, session,
                      session->FindDog(model::Dog::Id{0})});
    app::Application application(game, players, app::ApplicationMode::Manual, false, {}, repository);
    REQUIRE_THROWS_AS(application.JoinGame(map->GetId(), "Rejected"), std::overflow_error);
    REQUIRE(session->GetDogs().size() == 1);
    REQUIRE(players.GetPlayersList().size() == 1);
    REQUIRE(session->GetNextDogId() == 1);
}

TEST_CASE("Registration rollback removes the dog from every index without retirement")
{
    auto game = MakeReviewGame();
    const auto* map = game.FindMap(model::Map::Id{"review"});
    model::GameSession session(model::GameSession::Id{0}, map, {}, 60000);
    bool retired = false;
    session.SetDogRetirementHandler([&](const auto, const auto) { retired = true; });
    session.AddDog(MakeReviewDog());
    session.RemoveDog(model::Dog::Id{0});
    session.RemoveDog(model::Dog::Id{0});
    REQUIRE(session.GetDogs().empty());
    REQUIRE_FALSE(session.FindDog(model::Dog::Id{0}));
    REQUIRE(session.GetNextDogId() == 1);
    REQUIRE_FALSE(retired);
}

TEST_CASE("Domain constructors and configuration setters protect numeric invariants")
{
    model::Map map(model::Map::Id{"review"}, "Review");
    REQUIRE_THROWS_AS(map.SetBagCapacity(-1), std::invalid_argument);
    REQUIRE(map.GetBagCapacity() == 3);
    map.SetBagCapacity(0);
    REQUIRE(map.GetBagCapacity() == 0);
    REQUIRE_THROWS_AS(map.SetSpeed(-1.0), std::invalid_argument);
    REQUIRE_THROWS_AS(map.SetSpeed(std::numeric_limits<double>::infinity()), std::invalid_argument);
    REQUIRE_THROWS_AS(map.GetRandomPoint(), std::logic_error);
    REQUIRE_THROWS_AS(map.GetRandomLootType(), std::logic_error);
    REQUIRE_THROWS_AS(map.AddLoot(0, {0, -1}), std::invalid_argument);
    REQUIRE_THROWS_AS(map.AddLoot(0, {1, 1}), std::invalid_argument);
    REQUIRE_THROWS_AS(model::Dog(model::Dog::Id{0}, "Courier", {0, 0}, 1.0, "U", -1, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(model::Dog(model::Dog::Id{0}, "Courier", {0, 0}, 1.0, "invalid", 1, 0), std::invalid_argument);
    auto dog = MakeReviewDog();
    REQUIRE_THROWS_AS(dog.SetPosition({std::numeric_limits<double>::quiet_NaN(), 0}), std::invalid_argument);
    REQUIRE(geom::AreNear(dog.GetPosition().x, 0));
    model::Game game;
    REQUIRE_THROWS_AS(game.SetDogRetirementTime(0), std::invalid_argument);
    REQUIRE_THROWS_AS(game.SetLootGeneratorConfig({std::numeric_limits<double>::infinity(), 1.0}), std::invalid_argument);
}

TEST_CASE("Sparse loot type identifiers generate existing catalog entries")
{
    model::Map map(model::Map::Id{"review"}, "Sparse catalog");
    map.AddRoad({model::Road::HORIZONTAL, {0, 0}, 10});
    map.AddLoot(7, {7, 10});
    model::GameSession session(model::GameSession::Id{0}, &map, {1.0, 1.0}, 60000);
    session.AddDog(MakeReviewDog());
    session.UpdateState(1000);
    REQUIRE(session.GetLoots().size() == 1);
    REQUIRE(session.GetLoots().front().GetProps().type == 7);
}

TEST_CASE("Score overflow preserves the entire bag and previous score")
{
    auto dog = MakeReviewDog(0, 5);
    dog.PutLootInBag({model::Loot::Id{0}, {0, std::numeric_limits<int>::max()}, {0, 0}});
    dog.PutLootInBag({model::Loot::Id{1}, {0, 1}, {0, 0}});
    REQUIRE_THROWS_AS(dog.ExchangeLoot(), std::overflow_error);
    REQUIRE(dog.GetScore() == 5);
    REQUIRE(dog.GetLootList().size() == 2);
    auto valid = MakeReviewDog();
    valid.PutLootInBag({model::Loot::Id{0}, {0, std::numeric_limits<int>::max()}, {0, 0}});
    valid.ExchangeLoot();
    REQUIRE(valid.GetScore() == std::numeric_limits<int>::max());
    REQUIRE(valid.GetLootList().empty());
}

TEST_CASE("Collision geometry rejects zero paths and keeps small meaningful movement")
{
    REQUIRE_THROWS_AS(collision_detector::TryCollectPoint({0, 0}, {0, 0}, {0, 0}), std::invalid_argument);
    const auto collected = collision_detector::TryCollectPoint({0, 0}, {1e-8, 0}, {5e-9, 0});
    REQUIRE(collected.IsCollected(0.1));
    REQUIRE(geom::AreNear(collected.proj_ratio, 0.5));
}

TEST_CASE("Random tokens retain the public authorization format")
{
    for (int index = 0; index < 8; ++index)
    {
        REQUIRE(app::Application::IsValidToken(app::RandomToken::Generate()));
    }
}

TEST_CASE("Loot generator rejects invalid time and random source without corrupting accumulation")
{
    using Generator = loot_gen::LootGenerator;
    Generator generator(Generator::TimeInterval{1000}, 1.0);
    REQUIRE_THROWS_AS(generator.Generate(Generator::TimeInterval{-1}, 0, 1), std::invalid_argument);
    REQUIRE(generator.Generate(Generator::TimeInterval{1000}, 0, 1) == 1);
    Generator invalid(Generator::TimeInterval{1000}, 1.0,
                      []() { return std::numeric_limits<double>::quiet_NaN(); });
    REQUIRE_THROWS_AS(invalid.Generate(Generator::TimeInterval{1000}, 0, 1), std::invalid_argument);
}

TEST_CASE("Failed delivery does not duplicate picked loot on the map")
{
    model::Map map(model::Map::Id{"review"}, "Delivery failure");
    map.AddRoad({model::Road::HORIZONTAL, {0, 0}, 10});
    map.AddOffice({model::Office::Id{"office"}, {2, 0}, {0, 0}});
    model::GameSession session(model::GameSession::Id{0}, &map, {}, 60000);
    session.AddDog({model::Dog::Id{0}, "Courier", {0, 0}, 2.0, "R", 3, 1});
    const auto dog = session.FindDog(model::Dog::Id{0});
    dog->SetDirection("R");
    session.AddLoot({model::Loot::Id{0}, {0, std::numeric_limits<int>::max()}, {1, 0}});
    session.AddLoot({model::Loot::Id{1}, {0, 1}, {1.5, 0}});
    session.AddLoot({model::Loot::Id{2}, {0, 1}, {9, 0}});

    REQUIRE_THROWS_AS(session.UpdateState(1000), std::overflow_error);
    REQUIRE(dog->GetScore() == 1);
    REQUIRE(dog->GetLootList().size() == 2);
    REQUIRE(dog->GetLootList()[0].GetId() == model::Loot::Id{0});
    REQUIRE(dog->GetLootList()[1].GetId() == model::Loot::Id{1});
    REQUIRE(session.GetLoots().size() == 1);
    REQUIRE(session.GetLoots().front().GetId() == model::Loot::Id{2});

    REQUIRE_THROWS_AS(session.UpdateState(1000), std::overflow_error);
    REQUIRE(dog->GetLootList().size() == 2);
    REQUIRE(session.GetLoots().size() == 1);
}

TEST_CASE("Geometric proximity rejects nonfinite operands")
{
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for(const double invalid : {infinity, -infinity, nan})
    {
        REQUIRE_FALSE(geom::AreNear(invalid, 0.0));
        REQUIRE_FALSE(geom::AreNear(0.0, invalid));
        REQUIRE_FALSE(geom::AreNear(invalid, invalid));
    }
    REQUIRE(geom::AreNear(1.0, 1.0 + std::numeric_limits<double>::epsilon()));
    REQUIRE_FALSE(geom::AreNear(1.0, 1.001));
}

TEST_CASE("Overflowing movement still stops at the road boundary")
{
    model::Map map(model::Map::Id{"review"}, "Large finite speed");
    map.AddRoad({model::Road::HORIZONTAL, {0, 0}, 10});
    model::GameSession session(model::GameSession::Id{0}, &map, {}, 60000);
    session.AddDog({model::Dog::Id{0}, "Courier", {0, 0}, std::numeric_limits<double>::max(), "R", 3, 0});
    const auto dog = session.FindDog(model::Dog::Id{0});
    dog->SetDirection("R");
    session.UpdateState(2000);
    REQUIRE(geom::AreNear(dog->GetPosition().x, 10.4));
    REQUIRE(geom::AreNear(dog->GetSpeed().x, 0.0));
    REQUIRE(geom::AreNear(dog->GetSpeed().y, 0.0));
}
