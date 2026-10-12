#pragma once

#include <algorithm>
#include <cmath>
#include <compare>
#include <limits>

namespace geom
{
/** @brief Сравнивает вычисленные координаты с относительной машинной погрешностью.
 * @details Масштаб не меньше единицы, поэтому сравнение устойчиво около нуля.
 * Неконечные значения не считаются близкими, включая две одинаковые бесконечности.
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

/** @brief Двумерный вектор; для скорости компоненты заданы в единицах карты в секунду. */
struct Vec2D
{
    /** @brief Горизонтальная координата или компонента вектора. */
    double x = 0.0;
    /** @brief Вертикальная координата или компонента вектора. */
    double y = 0.0;
    /** @brief Создаёт нулевой вектор. */
    Vec2D() = default;
    /** @brief Создаёт вектор по компонентам. */
    Vec2D(const double x, const double y):
            x(x),
            y(y)
    {
    }
    /** @brief Масштабирует обе компоненты и возвращает изменённый вектор. */
    Vec2D& operator*=(const double scale)
    {
        x *= scale;
        y *= scale;
        return *this;
    }
    /** @brief Задаёт точный структурный порядок; геометрические проверки используют AreNear. */
    auto operator<=>(const Vec2D&) const = default;
};

/** @brief Возвращает новый вектор, умноженный на скаляр. */
inline Vec2D operator*(const Vec2D lhs, const double rhs)
{
    return {lhs.x * rhs, lhs.y * rhs};
}

/** @brief Поддерживает запись скаляра слева без изменения аргументов. */
inline Vec2D operator*(const double lhs, const Vec2D rhs)
{
    return rhs * lhs;
}

/** @brief Точка геометрического алгоритма столкновений в непрерывном пространстве. */
struct Point2D
{
    /** @brief Горизонтальная координата или компонента вектора. */
    double x = 0.0;
    /** @brief Вертикальная координата или компонента вектора. */
    double y = 0.0;
    /** @brief Создаёт точку начала координат. */
    Point2D() = default;
    /** @brief Создаёт точку по координатам. */
    Point2D(const double x, const double y):
            x(x),
            y(y)
    {
    }
    /** @brief Перемещает точку на вектор и возвращает изменённую точку. */
    Point2D& operator+=(const Vec2D& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }
    /** @brief Задаёт точный структурный порядок; геометрические проверки используют AreNear. */
    auto operator<=>(const Point2D&) const = default;
};

/** @brief Возвращает точку после переноса на вектор. */
inline Point2D operator+(const Point2D lhs, const Vec2D& rhs)
{
    return {lhs.x + rhs.x, lhs.y + rhs.y};
}

/** @brief Поддерживает симметричную запись переноса точки. */
inline Point2D operator+(const Vec2D& lhs, const Point2D rhs)
{
    return rhs + lhs;
}
} // namespace geom
