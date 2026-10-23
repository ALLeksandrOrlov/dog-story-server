#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/tagged.hpp"

#include "domain/geometry_types.hpp"
#include "domain/loot.hpp"

namespace model
{
/** @brief A straight road segment with a bounded movement area around its axis. */
class Road
{
public:
    /** @brief Constructor tag for a horizontal segment. */
    struct HorizontalTag
    {
    };
    /** @brief Constructor tag for a vertical segment. */
    struct VerticalTag
    {
    };
    /** @brief Selects the horizontal road constructor. */
    static constexpr HorizontalTag HORIZONTAL{};
    /** @brief Selects the vertical road constructor. */
    static constexpr VerticalTag VERTICAL{};

    /** @brief Creates a horizontal road; endpoints may be in either order. */
    Road(HorizontalTag, const Point start, const Coord end_x) noexcept;
    /** @brief Creates a vertical road; endpoints may be in either order. */
    Road(VerticalTag, const Point start, const Coord end_y) noexcept;
    /** @brief Checks whether the endpoints have equal integer Y coordinates. */
    bool IsHorizontal() const noexcept;
    /** @brief Checks whether the endpoints have equal integer X coordinates. */
    bool IsVertical() const noexcept;
    /** @brief Returns the start point from the configuration. */
    Point GetStart() const noexcept;
    /** @brief Returns the end point from the configuration. */
    Point GetEnd() const noexcept;
    /** @brief Checks membership in the road's closed movement area. */
    bool IsInArea(const Position point) const;
    /** @brief Clamps the target point to the nearest movement area boundary. */
    Position GetNearestPos(const Position target_pos) const;

private:
    /** @brief Full width of the road's traversable area. */
    static constexpr double m_width = 0.8;
    /** @brief Start of the road axis from the configuration. */
    Point m_start{};
    /** @brief End of the road axis from the configuration. */
    Point m_end{};
    /** @brief Minimum X coordinate of the traversable area. */
    double m_start_x_range = 0.0;
    /** @brief Maximum X coordinate of the traversable area. */
    double m_end_x_range = 0.0;
    /** @brief Minimum Y coordinate of the traversable area. */
    double m_start_y_range = 0.0;
    /** @brief Maximum Y coordinate of the traversable area. */
    double m_end_y_range = 0.0;
};

/** @brief A stationary rectangular building displayed by the client. */
class Building
{
public:
    /** @brief Stores the building bounds in map coordinates. */
    explicit Building(const Rectangle bounds) noexcept;
    /** @brief Returns the bounds; the reference remains valid for the building's lifetime. */
    const Rectangle& GetBounds() const noexcept;

private:
    /** @brief Building bounds used by the map view. */
    Rectangle m_bounds{};
};

/** @brief An item delivery point; position and image offset are independent. */
class Office
{
public:
    /** @brief A strongly typed entity identifier. */
    using Id = util::Tagged<std::string, Office>;
    /** @brief Creates a delivery point with a map-local identifier. */
    Office(const Id& id, const Point position, const Offset offset);
    /** @brief Returns the identifier, unique within the map. */
    const Id& GetId() const noexcept;
    /** @brief Returns the position used for delivery checks. */
    Point GetPosition() const noexcept;
    /** @brief Returns the image offset, which does not affect physics. */
    Offset GetOffset() const noexcept;

private:
    /** @brief The entity identifier, unchanged after registration. */
    Id m_id{std::string{}};
    /** @brief Current position in map coordinates. */
    Point m_position{};
    /** @brief Offset of the office image from the delivery point. */
    Offset m_offset{};
};

/** @brief Map configuration: geometry, movement rules, and item catalog.
 * @details Once transferred to Game, the map is read-only. Sessions hold
 * a non-owning reference to it; Game guarantees a stable address.
 */
class Map
{
public:
    /** @brief A strongly typed entity identifier. */
    using Id = util::Tagged<std::string, Map>;
    /** @brief Road segments in insertion order. */
    using Roads = std::vector<Road>;
    /** @brief Map buildings in insertion order. */
    using Buildings = std::vector<Building>;
    /** @brief Map offices in insertion order. */
    using Offices = std::vector<Office>;

    /** @brief Creates a configuration; roads must be added before a session is created. */
    Map(const Id& id, const std::string& name);
    /** @brief Returns the identifier, unique within the game. */
    const Id& GetId() const noexcept;
    /** @brief Returns the map's display name. */
    const std::string& GetName() const noexcept;
    /** @brief Provides buildings without allowing configuration changes. */
    const Buildings& GetBuildings() const noexcept;
    /** @brief Provides roads in configuration order. */
    const Roads& GetRoads() const noexcept;
    /** @brief Provides delivery points in configuration order. */
    const Offices& GetOffices() const noexcept;
    /** @brief Adds a road segment during map configuration. */
    void AddRoad(const Road& road);
    /** @brief Adds a building during map configuration. */
    void AddBuilding(const Building& building);
    /** @brief Adds an office while preserving identifier uniqueness.
     * @throws std::invalid_argument The identifier is already present.
     */
    void AddOffice(const Office& office);
    /** @brief Adds or replaces an item type; sparse type numbers are allowed.
     * @throws std::invalid_argument The type number does not match the parameters, or a value is negative.
     */
    void AddLoot(const int type, const LootProperties loot);
    /** @brief Finds a type description; returns std::nullopt if absent. */
    std::optional<LootProperties> GetLoot(const int type) const noexcept;
    /** @brief Selects an existing item type, including sparse type numbers.
     * @throws std::logic_error The item catalog is empty.
     */
    int GetRandomLootType() const;
    /** @brief Sets a finite, nonnegative speed in map units per second. */
    void SetSpeed(const double speed);
    /** @brief Returns the movement speed assigned to new dogs. */
    double GetSpeed() const noexcept;
    /** @brief Sets nonnegative capacity; zero prevents item collection. */
    void SetBagCapacity(const int capacity);
    /** @brief Returns the maximum number of items in the bag. */
    int GetBagCapacity() const noexcept;
    /** @brief Returns the number of registered types, not the highest type number. */
    int GetLootTypesNum() const noexcept;
    /** @brief Selects an integer point on the axis of a random road.
     * @throws std::logic_error The map has no roads.
     */
    Position GetRandomPoint() const;

private:
    /** @brief Maps an office identifier to its position in the array. */
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;
    /** @brief The entity identifier, unchanged after registration. */
    Id m_id{std::string{}};
    /** @brief The entity's display name. */
    std::string m_name{};
    /** @brief Geometry of traversable map areas. */
    Roads m_roads{};
    /** @brief Buildings displayed on the map. */
    Buildings m_buildings{};
    /** @brief Index of unique offices in the m_offices array. */
    OfficeIdToIndex m_office_id_to_index{};
    /** @brief Item delivery points. */
    Offices m_offices{};
    /** @brief Speed assigned to dogs when they join the map. */
    double m_dog_speed = 1.0;
    /** @brief Nonnegative maximum number of items. */
    int m_bag_capacity = 3;
    /** @brief Item type catalog; type numbers may be sparse. */
    std::unordered_map<int, LootProperties> m_loot_props{};
};
} // namespace model
