#pragma once
/**
 * @file
 * @brief UUID с меткой сущности и преобразования их строкового представления.
 */
#include <string>
#include <string_view>

#include <boost/uuid/uuid.hpp>

#include "common/tagged.hpp"

namespace util
{

/** @brief Адаптация Boost.UUID, используемая реализацией типизированных идентификаторов. */
namespace detail
{

using UUIDType = boost::uuids::uuid; ///< Внутреннее двоичное представление UUID.

/** @brief Создаёт случайный UUID средствами Boost.UUID. */
UUIDType NewUUID();

/** @brief Представляет UUID строкой в стандартном формате с дефисами. */
std::string UUIDToString(const UUIDType& uuid);
/** @brief Разбирает стандартную запись UUID; неверный формат приводит к исключению Boost. */
UUIDType UUIDFromString(const std::string_view text);

} // namespace detail

/** @brief UUID с меткой сущности; нулевое значение используется до десериализации. */
template <typename Tag> class TaggedUUID : public Tagged<detail::UUIDType, Tag>
{
public:
    using Base = Tagged<detail::UUIDType, Tag>; ///< Базовое хранение значения с той же меткой.
    using Tagged<detail::UUIDType, Tag>::Tagged;

    /** @brief Создаёт нулевой идентификатор. */
    TaggedUUID():
            Base{detail::UUIDType{}}
    {
    }

    /** @brief Возвращает новый случайный идентификатор этой сущности. */
    static TaggedUUID New()
    {
        return TaggedUUID{detail::NewUUID()};
    }

    /** @brief Разбирает строковую запись идентификатора с проверкой формата UUID. */
    static TaggedUUID FromString(const std::string& uuid_as_text)
    {
        return TaggedUUID{detail::UUIDFromString(uuid_as_text)};
    }

    /** @brief Возвращает стандартную строковую запись UUID с дефисами. */
    std::string ToString() const
    {
        return detail::UUIDToString(**this);
    }
};

} // namespace util
