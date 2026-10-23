#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "common/tagged.hpp"

#include "domain/geom.hpp"
#include "domain/loot.hpp"

namespace model
{
/** @brief A rejected time step that cannot be represented without overflow. */
class InvalidTimeDelta : public std::invalid_argument
{
public:
    /** @brief Passes the diagnostic message to the standard exception. */
    using std::invalid_argument::invalid_argument;
};

/** @brief A game dog's state: movement, bag, score, and lifetime.
 * @details A newly created dog is stationary; the stored direction specifies orientation,
 * not velocity. This preserves the snapshot restoration contract.
 */
class Dog
{
public:
    /** @brief A strongly typed entity identifier. */
    using Id = util::Tagged<std::uint32_t, Dog>;
    /** @brief Creates a stationary dog with the specified parameters.
     * @throws std::invalid_argument Position or speed is nonfinite, direction is invalid,
     * or speed, capacity, or score is negative.
     */
    Dog(const Id id, const std::string& name, const Position spawn_point, const double abs_speed,
            const std::string& direction, const int bag_capacity, const int score);
    /** @brief Returns the identifier, unique within the session. */
    const Id& GetId() const noexcept;
    /** @brief Returns the name supplied by the player when joining. */
    const std::string& GetName() const noexcept;
    /** @brief Sets finite coordinates; the session checks whether the position is on a road. */
    void SetPosition(const Position position);
    /** @brief Returns the current position in map coordinates. */
    Position GetPosition() const noexcept;
    /** @brief Returns the velocity vector in map units per second. */
    geom::Vec2D GetSpeed() const noexcept;
    /** @brief Returns the configured speed magnitude, including when stationary. */
    double GetAbsSpeed() const noexcept;
    /** @brief Changes movement direction; an empty string stops movement without changing orientation. */
    void SetDirection(const std::string& direction);
    /** @brief Returns the orientation: U, D, L, or R. */
    const std::string& GetDirection() const noexcept;
    /** @brief Puts an item in the bag if space is available; otherwise leaves state unchanged. */
    void PutLootInBag(const Loot& loot);
    /** @brief Delivers the entire bag and increases the score atomically.
     * @throws std::overflow_error The resulting score does not fit in int; the bag and score remain unchanged.
     */
    void ExchangeLoot();
    /** @brief Provides read access to the bag; the reference remains valid until the dog is modified. */
    const std::vector<Loot>& GetLootList() const noexcept;
    /** @brief Checks whether the bag can hold another item. */
    bool HasBagSpace() const noexcept;
    /** @brief Returns the maximum number of items in the bag. */
    int GetBagCapacity() const noexcept;
    /** @brief Returns the accumulated score for delivered items. */
    int GetScore() const noexcept;
    /** @brief Returns the lifetime in milliseconds. */
    int GetPlayTime() const noexcept;
    /** @brief Returns the continuous idle time in milliseconds. */
    int GetInactivityTime() const noexcept;
    /** @brief Validates the time step before the game world is modified. */
    void ValidateTimeDelta(const int time_delta) const;
    /** @brief Updates time counters; the session updates coordinates separately. */
    void UpdateState(const int time_delta);
    /** @brief Accepts U/D/L/R and an empty stop command. */
    static bool IsValidDirection(const std::string& direction);

private:
    /** @brief The entity's display name. */
    std::string m_name{};
    /** @brief The entity identifier, unchanged after registration. */
    Id m_id{0};
    /** @brief Current position in map coordinates. */
    Position m_position{};
    /** @brief Configured nonnegative speed magnitude. */
    double m_abs_speed = 0.0;
    /** @brief Current velocity vector; a zero vector means the dog is stationary. */
    geom::Vec2D m_map_speed{};
    /** @brief U/D/L/R orientation, preserved when stopped. */
    std::string m_dir{};
    /** @brief Nonnegative maximum number of items. */
    int m_bag_capacity = 0;
    /** @brief Items owned by the entity. */
    std::vector<Loot> m_loot_list{};
    /** @brief Accumulated nonnegative delivery score. */
    int m_score = 0;
    /** @brief The dog's lifetime in milliseconds. */
    int m_play_time = 0;
    /** @brief Continuous idle time in milliseconds. */
    int m_inactivity_time = 0;
};
} // namespace model
