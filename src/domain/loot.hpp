#pragma once

#include <cstdint>

#include "common/tagged.hpp"

#include "domain/geometry_types.hpp"

namespace model
{
/** @brief Item type and nonnegative delivery score. */
struct LootProperties
{
    /** @brief Type number in the map catalog. */
    int type = 0;
    /** @brief Nonnegative item delivery score. */
    int value = 0;
};

/** @brief An item with immutable identity, value, and spawn position. */
class Loot
{
public:
    /** @brief A strongly typed entity identifier. */
    using Id = util::Tagged<std::uint32_t, Loot>;

    /** @brief Creates an item; type, value, and coordinates must be valid.
     * @throws std::invalid_argument Type or value is negative, or position is nonfinite.
     */
    Loot(const Id id, const LootProperties loot_prop, const Position pos);
    /** @brief Returns the identifier, unique within the game session. */
    const Id& GetId() const noexcept;
    /** @brief Returns the parameters used when delivering the item. */
    LootProperties GetProps() const noexcept;
    /** @brief Returns the item's spawn point on the map. */
    Position GetPos() const noexcept;

private:
    /** @brief The entity identifier, unchanged after registration. */
    Id m_id{0};
    /** @brief Item type and delivery value. */
    LootProperties m_loot_prop{};
    /** @brief Item spawn point. */
    Position m_pos{};
};
} // namespace model
