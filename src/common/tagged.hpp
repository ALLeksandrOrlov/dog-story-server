#pragma once
/**
 * @file
 * @brief Строго типизированные значения и их хеширование.
 */
#include <compare>
#include <cstddef>
#include <functional>
#include <utility>

/** @brief Общие типы идентификаторов, не зависящие от игрового домена. */
namespace util
{

/** @brief Значение с меткой типа: идентификаторы разных сущностей нельзя случайно смешать.
 * @tparam Value Тип хранимого значения.
 * @tparam Tag Отличительная метка, не занимающая память в объекте.
 */
template <typename Value, typename Tag> class Tagged
{
public:
    using ValueType = Value; ///< Тип хранимого значения для обобщённых алгоритмов.
    using TagType = Tag; ///< Метка, различающая значения с одинаковым представлением.

    /** @brief Принимает значение перемещением. */
    explicit Tagged(Value&& value):
            m_value(std::move(value))
    {
    }
    /** @brief Копирует значение с явным указанием метки типа. */
    explicit Tagged(const Value& value):
            m_value(value)
    {
    }

    /** @brief Предоставляет неизменяемое значение без снятия метки неявным преобразованием. */
    const Value& operator*() const
    {
        return m_value;
    }

    /** @brief Предоставляет значение для десериализации; ключи контейнеров изменять нельзя. */
    Value& operator*()
    {
        return m_value;
    }

    /** @brief Сравнивает значения только с одинаковыми метками. */
    auto operator<=>(const Tagged<Value, Tag>&) const = default;

private:
    Value m_value; ///< Значение, обязательно инициализируемое одним из конструкторов.
};

/** @brief Хеширует значение типизированного идентификатора для unordered-контейнеров. */
template <typename TaggedValue> struct TaggedHasher
{
    /** @brief Возвращает хеш лежащего в основе значения. */
    std::size_t operator()(const TaggedValue& value) const
    {
        return std::hash<typename TaggedValue::ValueType>{}(*value);
    }
};

} // namespace util
