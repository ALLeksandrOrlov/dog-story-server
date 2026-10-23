#pragma once
/**
 * @file
 * @brief Entity-tagged UUIDs and conversions to and from their string representation.
 */
#include <string>
#include <string_view>

#include <boost/uuid/uuid.hpp>

#include "common/tagged.hpp"

namespace util
{

/** @brief Boost.UUID adapter used by the typed identifier implementation. */
namespace detail
{

using UUIDType = boost::uuids::uuid; ///< Internal binary UUID representation.

/** @brief Creates a random UUID using Boost.UUID. */
UUIDType NewUUID();

/** @brief Formats a UUID in the standard hyphenated form. */
std::string UUIDToString(const UUIDType& uuid);
/** @brief Parses a standard UUID string; invalid input causes a Boost exception. */
UUIDType UUIDFromString(const std::string_view text);

} // namespace detail

/** @brief Entity-tagged UUID; a nil value is used before deserialization. */
template <typename Tag> class TaggedUUID : public Tagged<detail::UUIDType, Tag>
{
public:
    using Base = Tagged<detail::UUIDType, Tag>; ///< Base value storage with the same tag.
    using Tagged<detail::UUIDType, Tag>::Tagged;

    /** @brief Creates a nil identifier. */
    TaggedUUID():
            Base{detail::UUIDType{}}
    {
    }

    /** @brief Returns a new random identifier for this entity type. */
    static TaggedUUID New()
    {
        return TaggedUUID{detail::NewUUID()};
    }

    /** @brief Parses an identifier string and validates the UUID format. */
    static TaggedUUID FromString(const std::string& uuid_as_text)
    {
        return TaggedUUID{detail::UUIDFromString(uuid_as_text)};
    }

    /** @brief Returns the standard hyphenated UUID string. */
    std::string ToString() const
    {
        return detail::UUIDToString(**this);
    }
};

} // namespace util
