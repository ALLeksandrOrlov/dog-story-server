#pragma once

#include <cstddef>
#include <vector>

#include "domain/geom.hpp"

namespace collision_detector
{
/** @brief Проекция предмета на траекторию собирателя и квадрат расстояния до неё. */
struct CollectionResult
{
    /** @brief Квадрат расстояния предмета до траектории. */
    double sq_distance = 0.0;
    /** @brief Доля траектории до ближайшей к предмету точки. */
    double proj_ratio = 0.0;
    /** @brief Проверяет попадание проекции на отрезок и в заданный радиус захвата. */
    bool IsCollected(const double collect_radius) const
    {
        return proj_ratio >= 0.0 && proj_ratio <= 1.0 && sq_distance <= collect_radius * collect_radius;
    }
};

/** @brief Вычисляет проекцию точки c на траекторию от a до b.
 * @pre Концы траектории различаются с учётом машинной погрешности.
 * @throws std::invalid_argument Траектория вырождена.
 */
CollectionResult TryCollectPoint(const geom::Point2D a, const geom::Point2D b, const geom::Point2D c);

/** @brief Неподвижный предмет или офис с радиусом захвата width. */
struct Item
{
    /** @brief Положение в координатах карты. */
    geom::Point2D position{};
    /** @brief Радиус захвата в единицах карты. */
    double width = 0.0;
};

/** @brief Траектория собаки за один шаг с радиусом захвата width. */
struct Gatherer
{
    /** @brief Положение собирателя в начале шага. */
    geom::Point2D start_pos{};
    /** @brief Положение собирателя в конце шага. */
    geom::Point2D end_pos{};
    /** @brief Радиус захвата в единицах карты. */
    double width = 0.0;
};

/** @brief Порт чтения геометрии; алгоритм не зависит от доменных контейнеров. */
class ItemGathererProvider
{
public:
    /** @brief Обеспечивает корректное уничтожение реализации через базовый интерфейс. */
    virtual ~ItemGathererProvider() = default;
    /** @brief Возвращает количество объектов, доступных через GetItem. */
    virtual size_t ItemsCount() const = 0;
    /** @brief Возвращает объект по индексу в диапазоне [0, ItemsCount()). */
    virtual Item GetItem(const size_t idx) const = 0;
    /** @brief Возвращает количество траекторий, доступных через GetGatherer. */
    virtual size_t GatherersCount() const = 0;
    /** @brief Возвращает траекторию по индексу в диапазоне [0, GatherersCount()). */
    virtual Gatherer GetGatherer(const size_t idx) const = 0;
};

/** @brief Возможный захват предмета; time — доля шага от 0 до 1. */
struct GatheringEvent
{
    /** @brief Индекс объекта в ItemGathererProvider. */
    size_t item_id = 0;
    /** @brief Индекс собирателя в ItemGathererProvider. */
    size_t gatherer_id = 0;
    /** @brief Квадрат расстояния предмета до траектории. */
    double sq_distance = 0.0;
    /** @brief Доля шага до события, от 0 до 1. */
    double time = 0.0;
};

/** @brief Находит захваты для невырожденных траекторий и сортирует их по времени.
 * @details Не изменяет предметы и не учитывает вместимость сумки: эти правила применяет сессия.
 */
std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider);
} // namespace collision_detector
