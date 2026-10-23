#pragma once

#include <algorithm>
#include <cmath>
#include <compare>
#include <limits>

namespace geom
{
/** @brief Compares computed coordinates using relative machine precision.
 * @details The scale is at least one, making the comparison stable near zero.
 * Nonfinite values are not considered close, including two identical infinities.
 */
inline bool AreNear(const double lhs, const double rhs)
{
    if (!std::isfinite(lhs) || !std::isfinite(rhs))
    {
        return false;
    }
    const double scale = std::max({1.0, std::abs(lhs), std::abs(rhs)});
    return std::abs(lhs - rhs) <= std::numeric_limits<double>::epsilon() * scale;
}

/** @brief A two-dimensional vector; velocity components are in map units per second. */
struct Vec2D
{
    /** @brief Horizontal coordinate or vector component. */
    double x = 0.0;
    /** @brief Vertical coordinate or vector component. */
    double y = 0.0;
    /** @brief Creates a zero vector. */
    Vec2D() = default;
    /** @brief Creates a vector from its components. */
    Vec2D(const double x, const double y):
            x(x),
            y(y)
    {
    }
    /** @brief Scales both components and returns the modified vector. */
    Vec2D& operator*=(const double scale)
    {
        x *= scale;
        y *= scale;
        return *this;
    }
    /** @brief Defines exact structural ordering; geometry checks use AreNear. */
    auto operator<=>(const Vec2D&) const = default;
};

/** @brief Returns a new vector multiplied by a scalar. */
inline Vec2D operator*(const Vec2D lhs, const double rhs)
{
    return {lhs.x * rhs, lhs.y * rhs};
}

/** @brief Supports a scalar on the left without modifying the arguments. */
inline Vec2D operator*(const double lhs, const Vec2D rhs)
{
    return rhs * lhs;
}

/** @brief A point in continuous space used by the collision algorithm. */
struct Point2D
{
    /** @brief Horizontal coordinate or vector component. */
    double x = 0.0;
    /** @brief Vertical coordinate or vector component. */
    double y = 0.0;
    /** @brief Creates a point at the origin. */
    Point2D() = default;
    /** @brief Creates a point from coordinates. */
    Point2D(const double x, const double y):
            x(x),
            y(y)
    {
    }
    /** @brief Translates the point by a vector and returns the modified point. */
    Point2D& operator+=(const Vec2D& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }
    /** @brief Defines exact structural ordering; geometry checks use AreNear. */
    auto operator<=>(const Point2D&) const = default;
};

/** @brief Returns a point translated by a vector. */
inline Point2D operator+(const Point2D lhs, const Vec2D& rhs)
{
    return {lhs.x + rhs.x, lhs.y + rhs.y};
}

/** @brief Supports symmetric notation for point translation. */
inline Point2D operator+(const Vec2D& lhs, const Point2D rhs)
{
    return rhs + lhs;
}
} // namespace geom
