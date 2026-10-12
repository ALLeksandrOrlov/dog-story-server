/**
 * @file
 * @brief Изолирует генерацию и строковый разбор UUID средствами Boost.
 */
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "common/tagged_uuid.hpp"

namespace util
{
namespace detail
{

UUIDType NewUUID()
{
    return boost::uuids::random_generator()();
}

std::string UUIDToString(const UUIDType& uuid)
{
    return to_string(uuid);
}

UUIDType UUIDFromString(const std::string_view text)
{
    boost::uuids::string_generator generator{};
    return generator(text.begin(), text.end());
}

} // namespace detail
} // namespace util
