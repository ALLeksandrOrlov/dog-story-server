#pragma once
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/tagged.h"
#include "domain/geom.h"
#include "domain/loot_generator.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

struct Position {
    double x, y;
};

struct LootProperties {
    int type;
    int value;
};

class Road

{
    struct HorizontalTag {
        HorizontalTag() = default;
    };

    struct VerticalTag {
        VerticalTag() = default;
    };

  public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept;

    Road(VerticalTag, Point start, Coord end_y) noexcept;

    bool IsHorizontal() const noexcept;

    bool IsVertical() const noexcept;

    Point GetStart() const noexcept;

    Point GetEnd() const noexcept;

    bool IsInArea(Position point) const;

    Position GetNearestPos(Position target_pos) const;

  private:
    Point start_;
    Point end_;
    const double width = 0.8;

    double start_x_range = 0.;
    double end_x_range = 0.;
    double start_y_range = 0.;
    double end_y_range = 0.;
};

class Building {
  public:
    explicit Building(Rectangle bounds) noexcept;

    const Rectangle& GetBounds() const noexcept;

  private:
    Rectangle bounds_;
};

class Office {
  public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept;

    const Id& GetId() const noexcept;

    Point GetPosition() const noexcept;

    Offset GetOffset() const noexcept;

  private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
  public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept;

    const Id& GetId() const noexcept;

    const std::string& GetName() const noexcept;

    const Buildings& GetBuildings() const noexcept;

    const Roads& GetRoads() const noexcept;

    const Offices& GetOffices() const noexcept;

    void AddRoad(const Road& road);

    void AddBuilding(const Building& building);

    void AddOffice(Office office);

    void AddLoot(int type, LootProperties loot);
    std::optional<LootProperties> GetLoot(int type) const noexcept;

    void SetSpeed(double speed);
    double GetSpeed() const noexcept;

    void SetBagCapacity(int capacity);
    int GetBagCapacity() const noexcept;

    int GetLootTypesNum() const noexcept;

    Position GetRandomPoint() const;

  private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;

    double dog_speed_ = 1.;
    int bag_capacity_ = 3;

    std::unordered_map<int, LootProperties> loot_props_;
};

class Loot {
  public:
    using Id = util::Tagged<std::uint32_t, Loot>;
    Loot(Id id, LootProperties loot_prop, Position pos);
    const Id& GetId() const noexcept;
    LootProperties GetProps() const noexcept;
    Position GetPos() const noexcept;

  private:
    Id id_{0};
    LootProperties loot_prop_;
    Position pos_;
};

class Dog {
  public:
    using Id = util::Tagged<std::uint32_t, Dog>;

    Dog(Id id, std::string name, Position spawn_point, double abs_speed, std::string direction,
        int bag_capacity, int score);

    const Id& GetId() const noexcept;

    const std::string& GetName() const noexcept;

    void SetPosition(Position position);
    Position GetPosition() const noexcept;

    geom::Vec2D GetSpeed() const noexcept;
    double GetAbsSpeed() const noexcept;

    void SetDirection(std::string dir_str);
    std::string GetDirection() const noexcept;

    void PutLootInBag(Loot loot);

    void ExchangeLoot();
    std::vector<Loot> GetLootList() const;

    int GetBagCapacity() const noexcept;

    int GetScore() const noexcept;

    int GetPlayTime() const noexcept;
    int GetInactivityTime() const noexcept;

    void ValidateTimeDelta(int time_delta) const;
    void UpdateState(int time_delta);

    static bool IsValidDirection(std::string dir_str);

  private:
    std::string name_;
    Id id_{0};
    Position position_;
    double abs_speed_;
    geom::Vec2D map_speed_;
    std::string dir_;
    int bag_capacity_;
    std::vector<Loot> loot_list_;
    int score_ = 0;
    int play_time_{0};
    int inactivity_time_{0};
};

struct LootGeneratorConfig {
    double period = 1.;
    double probability = 0.;
};

class InvalidTimeDelta : public std::invalid_argument {
  public:
    using std::invalid_argument::invalid_argument;
};

class GameSession {
  public:
    using Id = util::Tagged<std::uint32_t, GameSession>;
    using DogPtr = std::shared_ptr<Dog>;
    using Dogs = std::vector<DogPtr>;
    using Loots = std::deque<Loot>;

    explicit GameSession(Id id, const Map* map, LootGeneratorConfig loot_generator_config,
                         int dog_retirement_time);

    const Id& GetId() const noexcept;

    void AddDog(Dog dog);
    const Dogs& GetDogs() const;
    DogPtr FindDog(const Dog::Id& id) const;
    int GetNextDogId() const noexcept;

    void AddLoot(Loot loot);
    Loots GetLoots() const;
    int GetNextLootId() const noexcept;

    const Map* GetMap() const;

    void SetDogRetirementHandler(std::function<void(Id, Dog::Id)> on_dog_retirement);

    void UpdateState(int time_delta);

  private:
    std::pair<double, Position> GetCloserToPoint(Position current_pos, Position target_pos) const;

    void RemoveDog(size_t index);

    int GetRandomLootType();
    void RemoveLoot(size_t index);

    void UpdateDogsState(int time_delta);
    void UpdateCollisions(int time_delta);
    void UpdateLoot(int time_delta);

    using DogIdHasher = util::TaggedHasher<Dog::Id>;
    using DogIdToDog = std::unordered_map<Dog::Id, DogPtr, DogIdHasher>;

    Id id_{0};
    const Map* map_;

    Dogs dogs_;
    DogIdToDog dog_id_to_dog_;
    int next_dog_id_ = 0;

    Loots loot_list_;
    int next_loot_id_ = 0;
    LootGeneratorConfig loot_generator_config_;

    int dog_retirement_time_;
    loot_gen::LootGenerator loot_generator_;
    std::function<void(GameSession::Id, Dog::Id)> on_dog_retirement_;
};

class Game {
  public:
    using Maps = std::deque<Map>;
    Game() = default;
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&) = default;
    Game& operator=(Game&&) = default;
    using GameSessions = std::deque<GameSession>;

    void AddMap(Map map);
    const Maps& GetMaps() const noexcept;
    const Map* FindMap(const Map::Id& id) const noexcept;

    void AddSession(GameSession session);
    GameSessions& GetSessions();
    const GameSessions& GetSessions() const;
    GameSession* FindSession(const Map::Id& id) const noexcept;
    GameSession* FindSession(const GameSession::Id& id) const noexcept;
    int GetNextSessionId() const noexcept;

    void SetLootGeneratorConfig(LootGeneratorConfig config);
    const LootGeneratorConfig& GetLootGeneratorConfig() const;

    void SetDogRetirementTime(int time);
    int GetDogRetirementTime() const;

    void UpdateState(int time_delta);

  private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using SessionIdHasher = util::TaggedHasher<GameSession::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;
    using MapIdToSession = std::unordered_map<Map::Id, GameSession*, MapIdHasher>;
    using SessionIdToSession = std::unordered_map<GameSession::Id, GameSession*, SessionIdHasher>;

    Maps maps_;
    MapIdToIndex map_id_to_index_;

    GameSessions sessions_;
    MapIdToSession map_id_to_session_;
    SessionIdToSession session_id_to_session_;
    int next_session_id_ = 0;

    LootGeneratorConfig loot_generator_config_;

    int dog_retirement_time_ = 60000;
};

} // namespace model
