#pragma once

#include <algorithm>

#include <boost/serialization/vector.hpp>

#include "domain/game.hpp"

namespace model
{

/** @brief Сериализует свойства предмета в неизменном порядке полей Boost.Archive. */
template <typename Archive>
void serialize(Archive& ar, LootProperties& loot_props, [[maybe_unused]] const unsigned version)
{
    ar & loot_props.type;
    ar & loot_props.value;
}

/** @brief Сериализует координаты позиции без зависимости домена от Boost.Archive. */
template <typename Archive> void serialize(Archive& ar, Position& position, [[maybe_unused]] const unsigned version)
{
    ar & position.x;
    ar & position.y;
}

} // namespace model

namespace serialization
{
using namespace std::literals;

/** @brief Снимок предмета; отделяет архивный формат от поведения доменной сущности. */
class LootRepr
{
public:
    /** @brief Создаёт инициализированное представление для загрузки из архива. */
    LootRepr() = default;

    /** @brief Копирует только сохраняемые данные предмета. */
    explicit LootRepr(const model::Loot& loot):
            m_id(loot.GetId()),
            m_loot_prop(loot.GetProps()),
            m_pos(loot.GetPos())
    {
    }

    /** @brief Восстанавливает отдельный предмет без изменения архивного представления. */
    model::Loot Restore() const
    {
        return {m_id, m_loot_prop, m_pos};
    }

    /** @brief Читает или записывает поля в порядке архивного формата; имя задано протоколом Boost.Serialization. */
    template <typename Archive> void serialize(Archive& ar, [[maybe_unused]] const unsigned version)
    {
        /** @brief Идентификатор сохраняемой доменной сущности. */
        ar&* m_id;
        /** @brief Тип и стоимость предмета в архивном представлении. */
        ar & m_loot_prop;
        /** @brief Координаты предмета в единицах карты. */
        ar & m_pos;
    }

private:
    /** @brief Идентификатор сохраняемой доменной сущности. */
    model::Loot::Id m_id{0};
    /** @brief Тип и стоимость предмета в архивном представлении. */
    model::LootProperties m_loot_prop{};
    /** @brief Координаты предмета в единицах карты. */
    model::Position m_pos{};
};

/** @brief Хранит снимок игрока без скорости движения и таймеров. */
class DogRepr
{
public:
    /** @brief Создаёт безопасные значения до десериализации. */
    DogRepr() = default;

    /** @brief Фиксирует сохраняемые параметры игрока и содержимое его рюкзака. */
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

    /** @brief Создаёт неподвижного игрока с восстановленным рюкзаком и счётом. */
    model::Dog Restore() const
    {
        model::Dog dog(m_id, m_name, m_spawn_point, m_abs_speed, m_direction, m_bag_capacity, m_score);
        for(const auto& loot : m_loot_list)
        {
            dog.PutLootInBag(loot.Restore());
        }
        return dog;
    }

    /** @brief Читает или записывает поля в порядке архивного формата; имя задано протоколом Boost.Serialization. */
    template <typename Archive> void serialize(Archive& ar, [[maybe_unused]] const unsigned version)
    {
        /** @brief Идентификатор сохраняемой доменной сущности. */
        ar&* m_id;
        /** @brief Сохранённое имя игрока. */
        ar & m_name;
        /** @brief Позиция игрока на момент создания снимка, используемая при восстановлении. */
        ar & m_spawn_point;
        /** @brief Модуль скорости игрока в единицах карты в секунду. */
        ar & m_abs_speed;
        /** @brief Ориентация игрока U, D, L или R; не означает движение после загрузки. */
        ar & m_direction;
        /** @brief Неотрицательная максимальная вместимость сумки в предметах. */
        ar & m_bag_capacity;
        /** @brief Накопленный счёт за доставленные предметы. */
        ar & m_score;
        /** @brief Архивные представления предметов в сумке игрока. */
        ar & m_loot_list;
    }

private:
    /** @brief Идентификатор сохраняемой доменной сущности. */
    model::Dog::Id m_id{0};
    /** @brief Сохранённое имя игрока. */
    std::string m_name;
    /** @brief Позиция игрока на момент создания снимка, используемая при восстановлении. */
    model::Position m_spawn_point{};
    /** @brief Модуль скорости игрока в единицах карты в секунду. */
    double m_abs_speed = 0.0;
    /** @brief Ориентация игрока U, D, L или R; не означает движение после загрузки. */
    std::string m_direction;
    /** @brief Неотрицательная максимальная вместимость сумки в предметах. */
    int m_bag_capacity = 0;
    /** @brief Накопленный счёт за доставленные предметы. */
    int m_score = 0;
    /** @brief Архивные представления предметов в сумке игрока. */
    std::vector<LootRepr> m_loot_list;
};

/** @brief Представляет сессию через идентификатор карты и снимки её сущностей. */
class GameSessionRepr
{
public:
    /** @brief Создаёт пустое представление для десериализации. */
    GameSessionRepr() = default;

    /** @brief Копирует данные сессии, не сохраняя адреса объектов или обработчики. */
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
     * @brief Восстанавливает сессию на карте переданной игры.
     * @throws std::runtime_error Если карта отсутствует или позиция игрока недопустима.
     * @details Карта должна пережить возвращённую сессию.
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

    /** @brief Читает или записывает поля в порядке архивного формата; имя задано протоколом Boost.Serialization. */
    template <typename Archive> void serialize(Archive& ar, [[maybe_unused]] const unsigned version)
    {
        /** @brief Идентификатор сохраняемой доменной сущности. */
        ar&* m_id;
        /** @brief Идентификатор карты, которую должна предоставить восстанавливаемая игра. */
        ar&* m_map_id;
        /** @brief Снимки собак этой игровой сессии. */
        ar & m_dogs;
        /** @brief Снимки предметов, лежащих на карте сессии. */
        ar & m_loots;
    }

private:
    /** @brief Идентификатор сохраняемой доменной сущности. */
    model::GameSession::Id m_id{0};
    /** @brief Идентификатор карты, которую должна предоставить восстанавливаемая игра. */
    model::Map::Id m_map_id{""s};
    /** @brief Снимки собак этой игровой сессии. */
    std::vector<DogRepr> m_dogs;
    /** @brief Снимки предметов, лежащих на карте сессии. */
    std::vector<LootRepr> m_loots;
};

} // namespace serialization
