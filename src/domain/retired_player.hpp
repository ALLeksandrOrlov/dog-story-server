#pragma once

#include <string>
#include <vector>

#include "common/tagged_uuid.hpp"

namespace domain
{
/** @brief A finished game result for storage, independent of the active session. */
struct RetiredPlayer
{
    /** @brief Public identifier of the described entity. */
    std::string id{};
    /** @brief The entity's display name. */
    std::string name{};
    /** @brief Nonnegative score for delivered items. */
    int score = 0;
    /** @brief Duration of the finished game in milliseconds. */
    int play_time_ms = 0;
};
namespace detail
{
/** @brief Type tag for the unique identifier of a finished game. */
struct RetiredPlayerTag
{
};
} // namespace detail
/** @brief Unique identity of a finished game, independent of the active player ID. */
using RetiredPlayerId = util::TaggedUUID<detail::RetiredPlayerTag>;

/** @brief Storage port for finished games, independent of the database implementation.
 * @details Infrastructure errors propagate to the caller; the domain has no SQL dependencies.
 */
class RetiredPlayerRepository
{
public:
    /** @brief Enables safe ownership through the base interface. */
    virtual ~RetiredPlayerRepository() = default;
    /** @brief Saves the finished game before the active player is removed.
     * @details Successful return means this implementation has completed its storage operation.
     */
    virtual void SaveRetiredPlayer(const RetiredPlayer& player) = 0;
    /** @brief Reads a page of results ordered by descending score, then ascending time and name.
     * @pre offset and limit are nonnegative; the caller validates the page size.
     */
    virtual std::vector<RetiredPlayer> GetRetiredPlayers(const int offset, const int limit) = 0;
};
} // namespace domain
