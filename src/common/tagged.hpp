#pragma once
/**
 * @file
 * @brief Strongly typed values and their hash functions.
 */
#include <compare>
#include <cstddef>
#include <functional>
#include <utility>

/** @brief Common identifier types independent of the game domain. */
namespace util
{

/** @brief Tagged value that prevents accidental mixing of identifiers for different entities.
 * @tparam Value Type of the stored value.
 * @tparam Tag Distinguishing tag that occupies no memory in the object.
 */
template <typename Value, typename Tag> class Tagged
{
public:
    using ValueType = Value; ///< Stored value type for generic algorithms.
    using TagType = Tag;     ///< Tag distinguishing values with the same representation.

    /** @brief Takes the value by move. */
    explicit Tagged(Value&& value):
            m_value(std::move(value))
    {
    }
    /** @brief Copies a value with an explicitly specified type tag. */
    explicit Tagged(const Value& value):
            m_value(value)
    {
    }

    /** @brief Exposes the immutable value without stripping the tag through implicit conversion. */
    const Value& operator*() const
    {
        return m_value;
    }

    /** @brief Exposes the value for deserialization; container keys must not be modified. */
    Value& operator*()
    {
        return m_value;
    }

    /** @brief Compares only values with matching tags. */
    auto operator<=>(const Tagged<Value, Tag>&) const = default;

private:
    Value m_value; ///< Value that must be initialized by a constructor.
};

/** @brief Hashes a typed identifier for unordered containers. */
template <typename TaggedValue> struct TaggedHasher
{
    /** @brief Returns the hash of the underlying value. */
    std::size_t operator()(const TaggedValue& value) const
    {
        return std::hash<typename TaggedValue::ValueType>{}(*value);
    }
};

} // namespace util
