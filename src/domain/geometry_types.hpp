#pragma once

namespace model
{
/** @brief Integer dimension of map geometry. */
using Dimension = int;
/** @brief Integer coordinate of map geometry. */
using Coord = Dimension;

/** @brief Integer object position in map configuration. */
struct Point
{
    /** @brief Horizontal coordinate or vector component. */
    Coord x = 0;
    /** @brief Vertical coordinate or vector component. */
    Coord y = 0;
};

/** @brief Dimensions of a rectangular object in map coordinates. */
struct Size
{
    /** @brief Width in map units. */
    Dimension width = 0;
    /** @brief Height in map units. */
    Dimension height = 0;
};

/** @brief Building bounds: top-left corner and dimensions. */
struct Rectangle
{
    /** @brief Position in map coordinates. */
    Point position{};
    /** @brief Rectangle dimensions. */
    Size size{};
};

/** @brief Offset of the office image from its position. */
struct Offset
{
    /** @brief Horizontal image offset. */
    Dimension dx = 0;
    /** @brief Vertical image offset. */
    Dimension dy = 0;
};

/** @brief Continuous position of a moving object in map coordinates. */
struct Position
{
    /** @brief Horizontal coordinate or vector component. */
    double x = 0.0;
    /** @brief Vertical coordinate or vector component. */
    double y = 0.0;
};
} // namespace model
