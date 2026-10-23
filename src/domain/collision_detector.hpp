#pragma once

#include <cstddef>
#include <vector>

#include "domain/geom.hpp"

namespace collision_detector
{
/** @brief The projection of an item onto a gatherer's path and its squared distance from the path. */
struct CollectionResult
{
    /** @brief Squared distance from the item to the path. */
    double sq_distance = 0.0;
    /** @brief Fraction of the path to the point closest to the item. */
    double proj_ratio = 0.0;
    /** @brief Checks whether the projection lies on the segment and within the collection radius. */
    bool IsCollected(const double collect_radius) const
    {
        return proj_ratio >= 0.0 && proj_ratio <= 1.0 && sq_distance <= collect_radius * collect_radius;
    }
};

/** @brief Projects point c onto the path from a to b.
 * @pre The path endpoints are distinct after accounting for machine precision.
 * @throws std::invalid_argument The path is degenerate.
 */
CollectionResult TryCollectPoint(const geom::Point2D a, const geom::Point2D b, const geom::Point2D c);

/** @brief A stationary item or office with collection radius width. */
struct Item
{
    /** @brief Position in map coordinates. */
    geom::Point2D position{};
    /** @brief Collection radius in map units. */
    double width = 0.0;
};

/** @brief A dog's path over one step with collection radius width. */
struct Gatherer
{
    /** @brief The gatherer's position at the start of the step. */
    geom::Point2D start_pos{};
    /** @brief The gatherer's position at the end of the step. */
    geom::Point2D end_pos{};
    /** @brief Collection radius in map units. */
    double width = 0.0;
};

/** @brief Geometry read port; the algorithm is independent of domain containers. */
class ItemGathererProvider
{
public:
    /** @brief Enables correct destruction through the base interface. */
    virtual ~ItemGathererProvider() = default;
    /** @brief Returns the number of objects accessible through GetItem. */
    virtual size_t ItemsCount() const = 0;
    /** @brief Returns the object at an index in [0, ItemsCount()). */
    virtual Item GetItem(const size_t idx) const = 0;
    /** @brief Returns the number of paths accessible through GetGatherer. */
    virtual size_t GatherersCount() const = 0;
    /** @brief Returns the path at an index in [0, GatherersCount()). */
    virtual Gatherer GetGatherer(const size_t idx) const = 0;
};

/** @brief A possible item collection; time is the fraction of the step, from 0 to 1. */
struct GatheringEvent
{
    /** @brief Object index in ItemGathererProvider. */
    size_t item_id = 0;
    /** @brief Gatherer index in ItemGathererProvider. */
    size_t gatherer_id = 0;
    /** @brief Squared distance from the item to the path. */
    double sq_distance = 0.0;
    /** @brief Fraction of the step before the event, from 0 to 1. */
    double time = 0.0;
};

/** @brief Finds collection events along nondegenerate paths and sorts them by time.
 * @details Does not modify items or check bag capacity; the session applies these rules.
 */
std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider);
} // namespace collision_detector
