#include <limits>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "domain/collision_detector.hpp"
#include "domain/game_session.hpp"

TEST_CASE("A dog crosses all connected roads within one tick")
{
    const bool vertical = GENERATE(false, true);
    const bool backwards = GENERATE(false, true);
    INFO("vertical=" << vertical << ", backwards=" << backwards);
    model::Map map(model::Map::Id{"connected"}, "Connected roads");
    for(int start : {0, 10, 20})
    {
        if (vertical)
        {
            map.AddRoad({model::Road::VERTICAL, {0, start}, start + 10});
        }
        else
        {
            map.AddRoad({model::Road::HORIZONTAL, {start, 0}, start + 10});
        }
    }
    const double start = backwards ? 30.0 : 0.0;
    const model::Position position = vertical ? model::Position{0, start} : model::Position{start, 0};
    const std::string direction = vertical ? (backwards ? "U" : "D") : (backwards ? "L" : "R");
    model::GameSession session(model::GameSession::Id{0}, &map, {}, 60000);
    session.AddDog({model::Dog::Id{0}, "Courier", position, 25.0, direction, 3, 0});
    auto dog = session.FindDog(model::Dog::Id{0});
    dog->SetDirection(direction);

    session.UpdateState(1000);

    const double expected_position = backwards ? 5.0 : 25.0;
    REQUIRE((vertical ? dog->GetPosition().y : dog->GetPosition().x) == Catch::Approx(expected_position));
    REQUIRE((vertical ? dog->GetSpeed().y : dog->GetSpeed().x) == (backwards ? -25.0 : 25.0));

    session.UpdateState(1000);

    REQUIRE((vertical ? dog->GetPosition().y : dog->GetPosition().x) == Catch::Approx(backwards ? -0.4 : 30.4));
    REQUIRE(dog->GetSpeed().x == 0.0);
    REQUIRE(dog->GetSpeed().y == 0.0);
}

TEST_CASE("A dog cannot cross a gap between roads")
{
    model::Map map(model::Map::Id{"gap"}, "Disconnected roads");
    map.AddRoad({model::Road::HORIZONTAL, {0, 0}, 10});
    map.AddRoad({model::Road::HORIZONTAL, {20, 0}, 30});
    model::GameSession session(model::GameSession::Id{0}, &map, {}, 60000);
    session.AddDog({model::Dog::Id{0}, "Courier", {0, 0}, 25.0, "R", 3, 0});
    auto dog = session.FindDog(model::Dog::Id{0});
    dog->SetDirection("R");

    session.UpdateState(1000);

    REQUIRE(dog->GetPosition().x == Catch::Approx(10.4));
    REQUIRE(dog->GetSpeed().x == 0.0);
}

TEST_CASE("Overflowing movement follows connected roads to their final boundary")
{
    model::Map map(model::Map::Id{"fast"}, "Connected roads");
    for(int start : {0, 10, 20})
    {
        map.AddRoad({model::Road::HORIZONTAL, {start, 0}, start + 10});
    }
    model::GameSession session(model::GameSession::Id{0}, &map, {}, 60000);
    session.AddDog({model::Dog::Id{0}, "Courier", {0, 0}, std::numeric_limits<double>::max(), "R", 3, 0});
    auto dog = session.FindDog(model::Dog::Id{0});
    dog->SetDirection("R");

    session.UpdateState(2000);

    REQUIRE(dog->GetPosition().x == Catch::Approx(30.4));
    REQUIRE(dog->GetSpeed().x == 0.0);
}

TEST_CASE("Long collection paths preserve the perpendicular distance")
{
    SECTION("horizontal path")
    {
        const auto result = collision_detector::TryCollectPoint({0, 0}, {1000000000, 0}, {500000000, 1});
        REQUIRE(result.sq_distance == Catch::Approx(1.0));
        REQUIRE_FALSE(result.IsCollected(0.6));
    }
    SECTION("diagonal path")
    {
        const auto result = collision_detector::TryCollectPoint(
                {0, 0}, {1000000000, 1000000000}, {500000000, 500000001});
        REQUIRE(result.sq_distance == Catch::Approx(0.5));
        REQUIRE_FALSE(result.IsCollected(0.6));
    }
}

TEST_CASE("A long movement cannot collect an item on a parallel road out of reach")
{
    model::Map map(model::Map::Id{"parallel"}, "Parallel roads");
    map.AddRoad({model::Road::HORIZONTAL, {0, 0}, 1000000000});
    map.AddRoad({model::Road::HORIZONTAL, {0, 1}, 1000000000});
    model::GameSession session(model::GameSession::Id{0}, &map, {}, 60000);
    session.AddDog({model::Dog::Id{0}, "Courier", {0, 0}, 1000000000.0, "R", 3, 0});
    session.AddLoot({model::Loot::Id{0}, {0, 1}, {500000000, 1}});
    auto dog = session.FindDog(model::Dog::Id{0});
    dog->SetDirection("R");

    session.UpdateState(1000);

    REQUIRE(dog->GetLootList().empty());
    REQUIRE(session.GetLoots().size() == 1);
}

TEST_CASE("The first dog to reach an item collects it even when its movement is clipped")
{
    model::Map map(model::Map::Id{"race"}, "Road boundary");
    map.AddRoad({model::Road::HORIZONTAL, {0, 0}, 10});
    model::GameSession session(model::GameSession::Id{0}, &map, {}, 60000);
    session.AddDog({model::Dog::Id{0}, "Near", {8.5, 0}, 10.0, "R", 3, 0});
    session.AddDog({model::Dog::Id{1}, "Far", {10, 0}, 10.0, "L", 3, 0});
    session.AddLoot({model::Loot::Id{0}, {0, 1}, {9, 0}});
    auto near = session.FindDog(model::Dog::Id{0});
    auto far = session.FindDog(model::Dog::Id{1});
    near->SetDirection("R");
    far->SetDirection("L");

    session.UpdateState(1000);

    REQUIRE(near->GetLootList().size() == 1);
    REQUIRE(far->GetLootList().empty());
    REQUIRE(session.GetLoots().empty());
}
