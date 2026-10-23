#pragma once

#include <algorithm>

#include <boost/serialization/vector.hpp>

#include "domain/game.hpp"

namespace model
{

/** @brief Serializes loot properties in a fixed Boost.Archive field order. */
template <typename Archive>
void serialize(Archive& ar, LootProperties& loot_props, [[maybe_unused]] const unsigned version)
{
    ar & loot_props.type;
    ar & loot_props.value;
}

/** @brief Serializes position coordinates without making the domain depend on Boost.Archive. */
template <typename Archive> void serialize(Archive& ar, Position& position, [[maybe_unused]] const unsigned version)
{
    ar & position.x;
    ar & position.y;
}

} // namespace model

namespace serialization
{
using namespace std::literals;

/** @brief Loot snapshot separating the archive format from domain entity behavior. */
class LootRepr
{
public:
    /** @brief Creates an initialized representation for archive loading. */
    LootRepr() = default;

    /** @brief Copies only the persisted loot data. */
    explicit LootRepr(const model::Loot& loot):
            m_id(loot.GetId()),
            m_loot_prop(loot.GetProps()),
            m_pos(loot.GetPos())
    {
    }

    /** @brief Restores a separate loot item without changing the archive representation. */
    model::Loot Restore() const
    {
        return {m_id, m_loot_prop, m_pos};
    }

    /** @brief Reads or writes fields in archive order; Boost.Serialization requires this method name. */
    template <typename Archive> void serialize(Archive& ar, [[maybe_unused]] const unsigned version)
    {
        /** @brief Identifier of the persisted domain entity. */
        ar&* m_id;
        /** @brief Loot type and value in the archive representation. */
        ar & m_loot_prop;
        /** @brief Loot coordinates in map units. */
        ar & m_pos;
    }

private:
    /** @brief Identifier of the persisted domain entity. */
    model::Loot::Id m_id{0};
    /** @brief Loot type and value in the archive representation. */
    model::LootProperties m_loot_prop{};
    /** @brief Loot coordinates in map units. */
    model::Position m_pos{};
};

/** @brief Stores a player snapshot without velocity or timers. */
class DogRepr
{
public:
    /** @brief Initializes safe defaults before deserialization. */
    DogRepr() = default;

    /** @brief Captures persisted player properties and bag contents. */
    explicit DogRepr(const model::Dog& dog):
            m_id(dog.GetId()),
            m_name(dog.GetName()),
            m_spawn_point(dog.GetPosition()),
            m_abs_speed(dog.GetAbsSpeed()),
            m_direction(dog.GetDirection()),
            m_bag_capacity(dog.GetBagCapacity()),
            m_score(dog.GetScore())
    {
        const auto loot_list = dog.GetLootList();
        for(const auto& loot : loot_list)
        {
            m_loot_list.emplace_back(loot);
        }
    }

    /** @brief Creates a stationary player with restored bag contents and score. */
    model::Dog Restore() const
    {
        model::Dog dog(m_id, m_name, m_spawn_point, m_abs_speed, m_direction, m_bag_capacity, m_score);
        for(const auto& loot : m_loot_list)
        {
            dog.PutLootInBag(loot.Restore());
        }
        return dog;
    }

    /** @brief Reads or writes fields in archive order; Boost.Serialization requires this method name. */
    template <typename Archive> void serialize(Archive& ar, [[maybe_unused]] const unsigned version)
    {
        /** @brief Identifier of the persisted domain entity. */
        ar&* m_id;
        /** @brief Saved player name. */
        ar & m_name;
        /** @brief Player position at snapshot time, used during restoration. */
        ar & m_spawn_point;
        /** @brief Player speed magnitude in map units per second. */
        ar & m_abs_speed;
        /** @brief Player orientation U, D, L, or R; does not imply movement after loading. */
        ar & m_direction;
        /** @brief Nonnegative maximum number of items the bag can hold. */
        ar & m_bag_capacity;
        /** @brief Accumulated score for delivered items. */
        ar & m_score;
        /** @brief Archive representations of items in the player bag. */
        ar & m_loot_list;
    }

private:
    /** @brief Identifier of the persisted domain entity. */
    model::Dog::Id m_id{0};
    /** @brief Saved player name. */
    std::string m_name;
    /** @brief Player position at snapshot time, used during restoration. */
    model::Position m_spawn_point{};
    /** @brief Player speed magnitude in map units per second. */
    double m_abs_speed = 0.0;
    /** @brief Player orientation U, D, L, or R; does not imply movement after loading. */
    std::string m_direction;
    /** @brief Nonnegative maximum number of items the bag can hold. */
    int m_bag_capacity = 0;
    /** @brief Accumulated score for delivered items. */
    int m_score = 0;
    /** @brief Archive representations of items in the player bag. */
    std::vector<LootRepr> m_loot_list;
};

/** @brief Represents a session through its map identifier and entity snapshots. */
class GameSessionRepr
{
public:
    /** @brief Creates an empty representation for deserialization. */
    GameSessionRepr() = default;

    /** @brief Copies session data without storing object addresses or handlers. */
    explicit GameSessionRepr(const model::GameSession& session):
            m_id(session.GetId()),
            m_map_id(session.GetMap()->GetId())
    {
        const auto& dogs = session.GetDogs();
        for(const auto& dog : dogs)
        {
            m_dogs.emplace_back(*dog);
        }

        const auto& loots = session.GetLoots();
        for(const auto& loot : loots)
        {
            m_loots.emplace_back(loot);
        }
    }

    /**
     * @brief Restores a session on a map from the supplied game.
     * @throws std::runtime_error If the map is missing or a player position is invalid.
     * @details The map must outlive the returned session.
     */
    model::GameSession Restore(const model::Game& game) const
    {
        const model::Map* map = game.FindMap(m_map_id);
        if (!map)
        {
            throw std::runtime_error("Snapshot session references an unknown map");
        }
        model::GameSession session(m_id, map, game.GetLootGeneratorConfig(), game.GetDogRetirementTime());

        for(const auto& dog : m_dogs)
        {
            auto restored = dog.Restore();
            const auto position = restored.GetPosition();
            const auto& roads = map->GetRoads();
            if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
                    std::none_of(roads.begin(), roads.end(),
                            [position](const auto& road)
                            {
                                return road.IsInArea(position);
                            }))
            {
                throw std::runtime_error("Snapshot player position is outside map roads");
            }
            session.AddDog(std::move(restored));
        }

        for(const auto& loot : m_loots)
        {
            session.AddLoot(loot.Restore());
        }
        return session;
    }

    /** @brief Reads or writes fields in archive order; Boost.Serialization requires this method name. */
    template <typename Archive> void serialize(Archive& ar, [[maybe_unused]] const unsigned version)
    {
        /** @brief Identifier of the persisted domain entity. */
        ar&* m_id;
        /** @brief Identifier of the map that the restored game must provide. */
        ar&* m_map_id;
        /** @brief Dog snapshots for this game session. */
        ar & m_dogs;
        /** @brief Snapshots of loot items lying on the session map. */
        ar & m_loots;
    }

private:
    /** @brief Identifier of the persisted domain entity. */
    model::GameSession::Id m_id{0};
    /** @brief Identifier of the map that the restored game must provide. */
    model::Map::Id m_map_id{""s};
    /** @brief Dog snapshots for this game session. */
    std::vector<DogRepr> m_dogs;
    /** @brief Snapshots of loot items lying on the session map. */
    std::vector<LootRepr> m_loots;
};

} // namespace serialization
