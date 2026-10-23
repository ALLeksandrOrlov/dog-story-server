#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include "domain/collision_detector.hpp"
#include "domain/loot_generator.hpp"

#include "domain/game_session.hpp"

namespace model
{
using namespace std::literals;
using namespace collision_detector;
namespace
{
/** @brief Adapts a movement and item snapshot to the collision detection algorithm. */
class VectorItemGathererProvider : public collision_detector::ItemGathererProvider
{
public:
    /** @brief Accepts prepared arrays for one simulation step. */
    VectorItemGathererProvider(
            std::vector<collision_detector::Item> items, std::vector<collision_detector::Gatherer> gatherers):
            m_items(std::move(items)),
            m_gatherers(std::move(gatherers))
    {
    }

    /** @brief Returns the number of items and offices in the snapshot. */
    size_t ItemsCount() const override
    {
        return m_items.size();
    }

    /** @brief Returns a collision object with bounds checking. */
    collision_detector::Item GetItem(const size_t idx) const override
    {
        return m_items.at(idx);
    }

    /** @brief Returns the number of dog movements in the snapshot. */
    size_t GatherersCount() const override
    {
        return m_gatherers.size();
    }

    /** @brief Returns a dog's movement by index. */
    collision_detector::Gatherer GetGatherer(const size_t idx) const override
    {
        return m_gatherers[idx];
    }

private:
    /** @brief Item and office geometry for the current step. */
    std::vector<collision_detector::Item> m_items;
    /** @brief Dog paths for the current step. */
    std::vector<collision_detector::Gatherer> m_gatherers;
};

} // namespace
namespace
{
/** @brief Validates seconds before conversion to an integer duration. */
loot_gen::LootGenerator::TimeInterval ValidateLootPeriod(const LootGeneratorConfig config)
{
    constexpr double milliseconds_per_second = 1000.0;
    if (!std::isfinite(config.period) || config.period < 1.0 / milliseconds_per_second ||
            config.period > std::numeric_limits<int>::max() / milliseconds_per_second)
    {
        throw std::invalid_argument("Invalid loot generator period");
    }
    return std::chrono::duration_cast<loot_gen::LootGenerator::TimeInterval>(
            std::chrono::duration<double>(config.period));
}
} // namespace

GameSession::GameSession(
        const Id id, const Map* map, const LootGeneratorConfig loot_generator_config, const int dog_retirement_time):
        m_id(id),
        m_map(map),
        m_dog_retirement_time(dog_retirement_time),
        m_loot_generator(ValidateLootPeriod(loot_generator_config), loot_generator_config.probability)
{
    if (!m_map || m_map->GetRoads().empty() || dog_retirement_time <= 0)
    {
        throw std::invalid_argument("Session requires a map with roads and positive retirement time");
    }
}

const GameSession::Id& GameSession::GetId() const noexcept
{
    return m_id;
}

void GameSession::AddDog(const Dog& dog)
{
    const auto id = dog.GetId();
    if (m_dog_id_to_dog.contains(id))
    {
        throw std::invalid_argument("Duplicate dog id");
    }
    if (*id >= static_cast<unsigned>(std::numeric_limits<int>::max()))
    {
        throw std::invalid_argument("Dog id is outside the supported range");
    }
    auto next_dog = std::make_shared<Dog>(dog);
    m_dog_id_to_dog.emplace(id, next_dog);
    try
    {
        m_dogs.push_back(next_dog);
    }
    catch(...)
    {
        m_dog_id_to_dog.erase(id);
        throw;
    }
    m_next_dog_id = std::max(m_next_dog_id, static_cast<int>(*id) + 1);
}

const GameSession::Dogs& GameSession::GetDogs() const
{
    return m_dogs;
}

GameSession::DogPtr GameSession::FindDog(const Dog::Id& id) const
{
    if (m_dog_id_to_dog.count(id) > 0)
    {
        return m_dog_id_to_dog.at(id);
    }
    return nullptr;
}

int GameSession::GetNextDogId() const noexcept
{
    return m_next_dog_id;
}

void GameSession::AddLoot(const Loot& loot)
{
    const auto id = loot.GetId();
    if (std::any_of(m_loot_list.begin(), m_loot_list.end(),
                [id](const auto& item)
                {
                    return item.GetId() == id;
                }))
    {
        throw std::invalid_argument("Duplicate loot id");
    }
    if (*id >= static_cast<unsigned>(std::numeric_limits<int>::max()))
    {
        throw std::invalid_argument("Loot id is outside the supported range");
    }
    m_loot_list.emplace_back(loot);
    m_next_loot_id = std::max(m_next_loot_id, static_cast<int>(*id) + 1);
}

GameSession::Loots GameSession::GetLoots() const
{
    return m_loot_list;
}

int GameSession::GetNextLootId() const noexcept
{
    return m_next_loot_id;
}

const Map* GameSession::GetMap() const
{
    return m_map;
}

void GameSession::SetDogRetirementHandler(const std::function<void(Id, Dog::Id)>& on_dog_retirement)
{
    m_on_dog_retirement = on_dog_retirement;
}

void GameSession::UpdateState(const int time_delta)
{
    if (time_delta < 0)
    {
        throw InvalidTimeDelta("Time delta must be nonnegative");
    }
    for(const auto& dog : m_dogs)
    {
        dog->ValidateTimeDelta(time_delta);
    }
    UpdateDogsState(time_delta);
    UpdateCollisions(time_delta);
    UpdateLoot(time_delta);
}

std::pair<double, Position> GameSession::GetCloserToPoint(const Position current_pos, const Position target_pos) const
{
    const Map::Roads& roads = m_map->GetRoads();
    std::vector<std::pair<double, Position>> distances_to_target;
    for(const auto& road : roads)
    {
        if (road.IsInArea(current_pos))
        {
            Position next_point = road.GetNearestPos(target_pos);
            double distance = std::hypot(next_point.x - target_pos.x, next_point.y - target_pos.y);
            const bool is_target_reached = geom::AreNear(distance, 0.0);
            if (is_target_reached)
            {
                distances_to_target.clear();
                distances_to_target.push_back({distance, next_point});
                break;
            }
            distances_to_target.push_back({distance, next_point});
        }
    }

    if (distances_to_target.empty())
    {
        throw std::logic_error("Player position is outside map roads");
    }
    return *std::min_element(distances_to_target.begin(), distances_to_target.end(),
            [](const std::pair<double, Position>& a, const std::pair<double, Position>& b)
            {
                return a.first < b.first;
            });
}

void GameSession::RemoveDogAt(const size_t index)
{
    auto dog_id = m_dogs.at(index)->GetId();
    m_dogs.erase(m_dogs.begin() + index);
    m_dog_id_to_dog.erase(dog_id);
}

void GameSession::RemoveDog(const Dog::Id& id) noexcept
{
    const auto found = std::find_if(m_dogs.begin(), m_dogs.end(),
            [&id](const auto& dog)
            {
                return dog->GetId() == id;
            });
    if (found != m_dogs.end())
    {
        m_dogs.erase(found);
        m_dog_id_to_dog.erase(id);
    }
}

void GameSession::RemoveCollectedLoot(const std::vector<bool>& collected) noexcept
{
    static_assert(std::is_nothrow_move_assignable_v<Loot>);
    for(size_t index = collected.size(); index > 0; --index)
    {
        if (collected[index - 1])
        {
            m_loot_list.erase(m_loot_list.begin() + index - 1);
        }
    }
}

void GameSession::UpdateDogsState(const int time_delta)
{
    size_t idx = 0;
    std::vector<size_t> retired_dog_idxes;
    for(auto& dog : m_dogs)
    {
        dog->UpdateState(time_delta);
        if (dog->GetInactivityTime() >= m_dog_retirement_time)
        {
            retired_dog_idxes.push_back(idx);
        }
        idx++;
    }

    for(auto it = retired_dog_idxes.rbegin(); it != retired_dog_idxes.rend(); ++it)
    {
        const auto index = *it;
        if (m_on_dog_retirement)
        {
            m_on_dog_retirement(m_id, m_dogs.at(index)->GetId());
        }
        else
        {
            throw std::runtime_error("m_on_dog_retirement not set");
        }

        RemoveDogAt(index);
    }
}

void GameSession::UpdateCollisions(const int time_delta)
{
    using namespace std::literals;

    std::vector<Gatherer> gatherers;
    for(auto& dog : m_dogs)
    {
        Position pos = dog->GetPosition();
        geom::Vec2D speed = dog->GetSpeed();
        constexpr double milliseconds_per_second = 1000.0;
        Position target = {(pos.x + speed.x * (time_delta / milliseconds_per_second)),
                (pos.y + speed.y * (time_delta / milliseconds_per_second))};

        auto distance_pos = GetCloserToPoint(pos, target);
        if (!geom::AreNear(distance_pos.first, 0.0))
        {
            distance_pos = GetCloserToPoint(distance_pos.second, target);
        }

        if (!geom::AreNear(distance_pos.first, 0.0))
        {
            dog->SetDirection(""s);
        }
        dog->SetPosition(distance_pos.second);

        constexpr double dog_width = 0.6;
        gatherers.push_back({{pos.x, pos.y}, {distance_pos.second.x, distance_pos.second.y}, dog_width});
    }

    std::vector<Item> items;
    for(const auto& loot : m_loot_list)
    {
        Position pos = loot.GetPos();
        items.push_back({{pos.x, pos.y}, 0.});
    }
    for(const auto& office : m_map->GetOffices())
    {
        Point pos = office.GetPosition();
        constexpr double office_width = 0.5;
        items.push_back({{static_cast<double>(pos.x), static_cast<double>(pos.y)}, office_width});
    }

    VectorItemGathererProvider provider(std::move(items), std::move(gatherers));
    std::vector<GatheringEvent> events(FindGatherEvents(provider));

    // Allocate bookkeeping storage before changing bags so recording a successful pickup cannot throw.
    std::vector<bool> collected(m_loot_list.size(), false);
    try
    {
        for(const auto& event : events)
        {
            auto dog = m_dogs[event.gatherer_id];
            if (event.item_id < m_loot_list.size())
            {
                if (dog->HasBagSpace() && !collected[event.item_id])
                {
                    dog->PutLootInBag(m_loot_list[event.item_id]);
                    collected[event.item_id] = true;
                }
            }
            else
            {
                dog->ExchangeLoot();
            }
        }
    }
    catch(...)
    {
        // Even if a later delivery fails, an item cannot remain both on the map and in a bag.
        RemoveCollectedLoot(collected);
        throw;
    }
    RemoveCollectedLoot(collected);
}

void GameSession::UpdateLoot(const int time_delta)
{
    using namespace loot_gen;
    if (m_map->GetLootTypesNum() == 0)
    {
        return;
    }
    unsigned new_loots_num = m_loot_generator.Generate(LootGenerator::TimeInterval(time_delta),
            static_cast<unsigned>(m_loot_list.size()), static_cast<unsigned>(m_dogs.size()));
    for(; new_loots_num > 0; new_loots_num--)
    {
        const int random_loot_type = m_map->GetRandomLootType();
        Position random_point = m_map->GetRandomPoint();
        auto loot_id = Loot::Id(GetNextLootId());
        auto loot_opt = m_map->GetLoot(random_loot_type);
        AddLoot(Loot(loot_id, *loot_opt, random_point));
    }
}

} // namespace model
