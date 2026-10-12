#include <algorithm>
#include <cmath>
#include <limits>

#include "domain/loot_generator.hpp"

namespace loot_gen
{

unsigned LootGenerator::Generate(const TimeInterval time_delta, const unsigned loot_count, const unsigned looter_count)
{
    if (time_delta.count() < 0)
    {
        throw std::invalid_argument("Time delta must be nonnegative");
    }
    if (time_delta.count() > std::numeric_limits<TimeInterval::rep>::max() - m_time_without_loot.count())
    {
        throw std::overflow_error("Loot generation interval overflow");
    }
    const double random_coefficient = m_random_generator();
    if (!std::isfinite(random_coefficient) || random_coefficient < 0.0 || random_coefficient > 1.0)
    {
        throw std::invalid_argument("Random coefficient must be between zero and one");
    }
    m_time_without_loot += time_delta;
    const unsigned loot_shortage = (loot_count > looter_count) ? 0u : looter_count - loot_count;
    const double ratio = std::chrono::duration<double>{m_time_without_loot} / m_base_interval;
    const double probability = std::clamp((1.0 - std::pow(1.0 - m_probability, ratio)) * random_coefficient, 0.0, 1.0);
    const unsigned generated_loot = static_cast<unsigned>(std::round(loot_shortage * probability));
    if (generated_loot > 0)
    {
        m_time_without_loot = {};
    }
    return generated_loot;
}

} // namespace loot_gen
