#pragma once

#include <chrono>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace loot_gen
{
/** @brief Accumulates time and computes replenishment up to the number of dogs.
 * @details One instance belongs to one session and persists between ticks.
 * The algorithm does not select item types or positions and is independent of the map.
 */
class LootGenerator
{
public:
    /** @brief Source of a probability factor in [0, 1]. */
    using RandomGenerator = std::function<double()>;
    /** @brief Duration in milliseconds. */
    using TimeInterval = std::chrono::milliseconds;
    /** @brief Configures the replenishment interval and probability factor source.
     * @param base_interval Positive base interval.
     * @param probability Replenishment probability per base interval, from 0 to 1.
     * @param random_gen Returns a factor in [0, 1].
     * @throws std::invalid_argument Duration or probability is invalid, or the generator is empty.
     */
    LootGenerator(const TimeInterval base_interval, const double probability,
            const RandomGenerator& random_gen = DefaultGenerator):
            m_base_interval(base_interval),
            m_probability(probability),
            m_random_generator(random_gen)
    {
        if (m_base_interval.count() <= 0 || !std::isfinite(m_probability) || m_probability < 0.0 ||
                m_probability > 1.0 || !m_random_generator)
        {
            throw std::invalid_argument("Invalid loot generator configuration");
        }
    }
    /** @brief Returns the number of new items, capped by the shortage relative to the dog count.
     * @param time_delta Nonnegative time step.
     * @param loot_count Number of items on the map before replenishment.
     * @param looter_count Number of dogs in the session.
     * @details Accumulated time is reset only when at least one item is added.
     * @throws std::invalid_argument The step is negative or the generator returns an invalid factor.
     * @throws std::overflow_error The accumulated duration is not representable.
     */
    unsigned Generate(const TimeInterval time_delta, const unsigned loot_count, const unsigned looter_count);

private:
    /** @brief Base interval for the configured probability. */
    TimeInterval m_base_interval{};
    /** @brief Replenishment probability per base interval. */
    double m_probability = 0.0;
    /** @brief Time accumulated since the last nonzero replenishment. */
    TimeInterval m_time_without_loot{};
    /** @brief Source of the probability factor, independent of random geometry. */
    RandomGenerator m_random_generator{};
    /** @brief Returns the neutral factor for the standard deterministic calculation. */
    static double DefaultGenerator() noexcept
    {
        return 1.0;
    }
};
} // namespace loot_gen
