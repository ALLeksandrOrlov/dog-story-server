#pragma once

#include <cstdint>

#include "common/tagged.hpp"

#include "domain/geometry_types.hpp"

namespace model
{
/** @brief Тип предмета и неотрицательное число очков за его доставку. */
struct LootProperties
{
    /** @brief Номер типа в каталоге карты. */
    int type = 0;
    /** @brief Неотрицательные очки за доставку предмета. */
    int value = 0;
};

/** @brief Предмет с неизменяемой идентичностью, стоимостью и позицией появления. */
class Loot
{
public:
    /** @brief Строго типизированный идентификатор сущности. */
    using Id = util::Tagged<std::uint32_t, Loot>;

    /** @brief Создаёт предмет; тип, стоимость и координаты должны быть допустимыми.
     * @throws std::invalid_argument Отрицательный тип/стоимость либо неконечная позиция.
     */
    Loot(const Id id, const LootProperties loot_prop, const Position pos);
    /** @brief Возвращает идентификатор, уникальный в игровой сессии. */
    const Id& GetId() const noexcept;
    /** @brief Возвращает параметры, используемые при доставке предмета. */
    LootProperties GetProps() const noexcept;
    /** @brief Возвращает точку появления предмета на карте. */
    Position GetPos() const noexcept;

private:
    /** @brief Идентификатор сущности, не изменяемый после регистрации. */
    Id m_id{0};
    /** @brief Тип предмета и стоимость доставки. */
    LootProperties m_loot_prop{};
    /** @brief Точка появления предмета. */
    Position m_pos{};
};
} // namespace model
