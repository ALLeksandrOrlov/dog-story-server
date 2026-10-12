#pragma once

#include <chrono>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace loot_gen
{
/** @brief Накапливает время и вычисляет пополнение предметов до числа собак.
 * @details Один экземпляр принадлежит одной сессии и сохраняется между тактами.
 * Алгоритм не выбирает типы и позиции предметов и не зависит от карты.
 */
class LootGenerator
{
public:
    /** @brief Источник коэффициента вероятности в диапазоне [0, 1]. */
    using RandomGenerator = std::function<double()>;
    /** @brief Длительность в миллисекундах. */
    using TimeInterval = std::chrono::milliseconds;
    /** @brief Настраивает частоту пополнения и источник коэффициента вероятности.
     * @param base_interval Положительный базовый интервал.
     * @param probability Вероятность пополнения за базовый интервал, от 0 до 1.
     * @param random_gen Возвращает коэффициент в диапазоне [0, 1].
     * @throws std::invalid_argument Недопустимая длительность, вероятность или пустой генератор.
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
    /** @brief Возвращает число новых предметов, не превышающее дефицит относительно собак.
     * @param time_delta Неотрицательный шаг времени.
     * @param loot_count Число предметов на карте до пополнения.
     * @param looter_count Число собак в сессии.
     * @details Время накопления сбрасывается только при ненулевом пополнении.
     * @throws std::invalid_argument Отрицательный шаг или недопустимый коэффициент генератора.
     * @throws std::overflow_error Накопленная длительность не представима.
     */
    unsigned Generate(const TimeInterval time_delta, const unsigned loot_count, const unsigned looter_count);

private:
    /** @brief Базовая длительность для заданной вероятности. */
    TimeInterval m_base_interval{};
    /** @brief Вероятность пополнения за базовый интервал. */
    double m_probability = 0.0;
    /** @brief Накопленное время с последнего ненулевого пополнения. */
    TimeInterval m_time_without_loot{};
    /** @brief Источник коэффициента вероятности, не геометрической случайности. */
    RandomGenerator m_random_generator{};
    /** @brief Возвращает нейтральный коэффициент для стандартного детерминированного расчёта. */
    static double DefaultGenerator() noexcept
    {
        return 1.0;
    }
};
} // namespace loot_gen
