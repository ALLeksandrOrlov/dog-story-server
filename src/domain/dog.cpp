#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include "domain/dog.hpp"

namespace model
{
using namespace std::literals;
Dog::Dog(const Id id, const std::string& name, const Position spawn_point, const double abs_speed,
        const std::string& direction, const int bag_capacity, const int score):
        m_name(name),
        m_id(id),
        m_position(spawn_point),
        m_abs_speed(abs_speed),
        m_dir(direction),
        m_bag_capacity(bag_capacity),
        m_score(score)
{
    if (!std::isfinite(spawn_point.x) || !std::isfinite(spawn_point.y) || !std::isfinite(abs_speed) ||
            abs_speed < 0.0 || bag_capacity < 0 || score < 0 || direction.empty() || !IsValidDirection(direction))
    {
        throw std::invalid_argument("Invalid dog state");
    }
}

const Dog::Id& Dog::GetId() const noexcept
{
    return m_id;
}

const std::string& Dog::GetName() const noexcept
{
    return m_name;
}

void Dog::SetPosition(const Position position)
{
    if (!std::isfinite(position.x) || !std::isfinite(position.y))
    {
        throw std::invalid_argument("Dog position must be finite");
    }
    m_position = position;
}

Position Dog::GetPosition() const noexcept
{
    return m_position;
}

geom::Vec2D Dog::GetSpeed() const noexcept
{
    return m_map_speed;
}

double Dog::GetAbsSpeed() const noexcept
{
    return m_abs_speed;
}

void Dog::SetDirection(const std::string& dir)
{
    using namespace std::literals;
    if (dir == "U"s)
    {
        m_dir = dir;
        m_map_speed = {0., -m_abs_speed};
    }
    else if (dir == "D"s)
    {
        m_dir = dir;
        m_map_speed = {0., m_abs_speed};
    }
    else if (dir == "L"s)
    {
        m_dir = dir;
        m_map_speed = {-m_abs_speed, 0.};
    }
    else if (dir == "R"s)
    {
        m_dir = dir;
        m_map_speed = {m_abs_speed, 0.};
    }
    else if (dir.empty())
    {
        m_map_speed = {0., 0.};
    }
    else
    {
        throw std::runtime_error("Invalid direction");
    }
}

const std::string& Dog::GetDirection() const noexcept
{
    return m_dir;
}

void Dog::PutLootInBag(const Loot& loot)
{
    if (HasBagSpace())
    {
        m_loot_list.emplace_back(loot);
    }
}

void Dog::ExchangeLoot()
{
    std::int64_t next_score = m_score;
    for(const auto& loot : m_loot_list)
    {
        next_score += loot.GetProps().value;
        if (next_score > std::numeric_limits<int>::max())
        {
            throw std::overflow_error("Delivered loot exceeds the supported score range");
        }
    }
    m_score = static_cast<int>(next_score);
    m_loot_list.clear();
}

const std::vector<Loot>& Dog::GetLootList() const noexcept
{
    return m_loot_list;
}

bool Dog::HasBagSpace() const noexcept
{
    return m_loot_list.size() < static_cast<size_t>(m_bag_capacity);
}

int Dog::GetBagCapacity() const noexcept
{
    return m_bag_capacity;
}

int Dog::GetScore() const noexcept
{
    return m_score;
}

int Dog::GetPlayTime() const noexcept
{
    return m_play_time;
}

int Dog::GetInactivityTime() const noexcept
{
    return m_inactivity_time;
}

void Dog::ValidateTimeDelta(const int time_delta) const
{
    if (time_delta < 0 || m_play_time > std::numeric_limits<int>::max() - time_delta)
    {
        throw InvalidTimeDelta("Time delta is outside the supported range");
    }
}

void Dog::UpdateState(const int time_delta)
{
    ValidateTimeDelta(time_delta);
    m_play_time += time_delta;

    if (!geom::AreNear(m_map_speed.x, 0.0) || !geom::AreNear(m_map_speed.y, 0.0))
    {
        m_inactivity_time = 0;
    }
    else
    {
        m_inactivity_time += time_delta;
    }
}

bool Dog::IsValidDirection(const std::string& dir)
{
    using namespace std::literals;
    if ((dir == "U"s) || (dir == "D"s) || (dir == "L"s) || (dir == "R"s) || dir.empty())
    {
        return true;
    }
    return false;
}

Loot::Loot(const Id id, const LootProperties loot_prop, const Position pos):
        m_id(id),
        m_loot_prop(loot_prop),
        m_pos(pos)
{
    if (loot_prop.type < 0 || loot_prop.value < 0 || !std::isfinite(pos.x) || !std::isfinite(pos.y))
    {
        throw std::invalid_argument("Invalid loot properties or position");
    }
}

const Loot::Id& Loot::GetId() const noexcept
{
    return m_id;
}

LootProperties Loot::GetProps() const noexcept
{
    return m_loot_prop;
}

Position Loot::GetPos() const noexcept
{
    return m_pos;
}

} // namespace model
