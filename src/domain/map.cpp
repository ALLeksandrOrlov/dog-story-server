#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <random>
#include <stdexcept>

#include "domain/map.hpp"

namespace model
{
using namespace std::literals;
Road::Road(HorizontalTag, const Point start, const Coord end_x) noexcept:
        m_start{start},
        m_end{end_x, start.y}
{
    m_start_x_range = static_cast<double>(std::min(m_start.x, m_end.x)) - m_width / 2.;
    m_end_x_range = static_cast<double>(std::max(m_start.x, m_end.x)) + m_width / 2.;
    m_start_y_range = static_cast<double>(m_start.y) - m_width / 2.;
    m_end_y_range = static_cast<double>(m_end.y) + m_width / 2.;
}

Road::Road(VerticalTag, const Point start, const Coord end_y) noexcept:
        m_start{start},
        m_end{start.x, end_y}
{
    m_start_x_range = static_cast<double>(m_start.x) - m_width / 2.;
    m_end_x_range = static_cast<double>(m_end.x) + m_width / 2.;
    m_start_y_range = static_cast<double>(std::min(m_start.y, m_end.y)) - m_width / 2.;
    m_end_y_range = static_cast<double>(std::max(m_start.y, m_end.y)) + m_width / 2.;
}

bool Road::IsHorizontal() const noexcept
{
    return m_start.y == m_end.y;
}

bool Road::IsVertical() const noexcept
{
    return m_start.x == m_end.x;
}

Point Road::GetStart() const noexcept
{
    return m_start;
}

Point Road::GetEnd() const noexcept
{
    return m_end;
}

bool Road::IsInArea(const Position point) const
{
    if ((point.x < m_start_x_range) || (point.x > m_end_x_range))
    {
        return false;
    }

    if ((point.y < m_start_y_range) || (point.y > m_end_y_range))
    {
        return false;
    }

    return true;
}

Position Road::GetNearestPos(const Position target_pos) const
{
    Position pos = target_pos;
    if (pos.x < m_start_x_range)
    {
        pos.x = m_start_x_range;
    }
    else if (pos.x > m_end_x_range)
    {
        pos.x = m_end_x_range;
    }

    if (pos.y < m_start_y_range)
    {
        pos.y = m_start_y_range;
    }
    else if (pos.y > m_end_y_range)
    {
        pos.y = m_end_y_range;
    }
    return pos;
}

Building::Building(const Rectangle bounds) noexcept:
        m_bounds{bounds}
{
}

const Rectangle& Building::GetBounds() const noexcept
{
    return m_bounds;
}

Office::Office(const Id& id, const Point position, const Offset offset):
        m_id{id},
        m_position{position},
        m_offset{offset}
{
}

const Office::Id& Office::GetId() const noexcept
{
    return m_id;
}

Point Office::GetPosition() const noexcept
{
    return m_position;
}

Offset Office::GetOffset() const noexcept
{
    return m_offset;
}

Map::Map(const Id& id, const std::string& name):
        m_id(id),
        m_name(name)
{
}

const Map::Id& Map::GetId() const noexcept
{
    return m_id;
}

const std::string& Map::GetName() const noexcept
{
    return m_name;
}

const Map::Buildings& Map::GetBuildings() const noexcept
{
    return m_buildings;
}

const Map::Roads& Map::GetRoads() const noexcept
{
    return m_roads;
}

const Map::Offices& Map::GetOffices() const noexcept
{
    return m_offices;
}

void Map::AddRoad(const Road& road)
{
    m_roads.emplace_back(road);
}

void Map::AddBuilding(const Building& building)
{
    m_buildings.emplace_back(building);
}

void Map::AddOffice(const Office& office)
{
    if (m_office_id_to_index.contains(office.GetId()))
    {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = m_offices.size();
    Office& o = m_offices.emplace_back(office);
    try
    {
        m_office_id_to_index.emplace(o.GetId(), index);
    }
    catch(std::exception&)
    {
        // Remove the office from the vector if insertion into unordered_map fails.
        m_offices.pop_back();
        throw;
    }
}

void Map::AddLoot(const int type, const LootProperties loot)
{
    if (type < 0 || loot.type != type || loot.value < 0)
    {
        throw std::invalid_argument("Invalid loot type or value");
    }
    if (!m_loot_props.contains(type) && m_loot_props.size() >= static_cast<size_t>(std::numeric_limits<int>::max()))
    {
        throw std::overflow_error("Too many loot types");
    }
    m_loot_props.insert_or_assign(type, loot);
}

std::optional<LootProperties> Map::GetLoot(const int type) const noexcept
{
    if (m_loot_props.count(type) > 0)
    {
        return m_loot_props.at(type);
    }
    return std::nullopt;
}

void Map::SetSpeed(const double speed)
{
    if (!std::isfinite(speed) || speed < 0.0)
    {
        throw std::invalid_argument("Dog speed must be finite and nonnegative");
    }
    m_dog_speed = speed;
}

double Map::GetSpeed() const noexcept
{
    return m_dog_speed;
}

void Map::SetBagCapacity(const int capacity)
{
    if (capacity < 0)
    {
        throw std::invalid_argument("Bag capacity must be nonnegative");
    }
    m_bag_capacity = capacity;
}

int Map::GetBagCapacity() const noexcept
{
    return m_bag_capacity;
}

int Map::GetLootTypesNum() const noexcept
{
    return static_cast<int>(m_loot_props.size());
}

int Map::GetRandomLootType() const
{
    if (m_loot_props.empty())
    {
        throw std::logic_error("Cannot choose a loot type from an empty catalog");
    }
    std::random_device random_device;
    std::uniform_int_distribution<size_t> distribution(0, m_loot_props.size() - 1);
    auto selected = m_loot_props.cbegin();
    std::advance(selected, distribution(random_device));
    return selected->first;
}

Position Map::GetRandomPoint() const
{
    if (m_roads.empty())
    {
        throw std::logic_error("Cannot choose a point on a map without roads");
    }
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, static_cast<int>(m_roads.size()) - 1);
    auto& random_road = m_roads[dist(rd)];

    int x1 = std::min(random_road.GetStart().x, random_road.GetEnd().x);
    int y1 = std::min(random_road.GetStart().y, random_road.GetEnd().y);
    int x2 = std::max(random_road.GetStart().x, random_road.GetEnd().x);
    int y2 = std::max(random_road.GetStart().y, random_road.GetEnd().y);

    std::uniform_int_distribution<int> dist_x(x1, x2);
    std::uniform_int_distribution<int> dist_y(y1, y2);

    double x_random = static_cast<double>(dist_x(rd));
    double y_random = static_cast<double>(dist_y(rd));

    return {x_random, y_random};
}

} // namespace model
