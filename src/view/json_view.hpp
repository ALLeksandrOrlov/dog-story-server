#pragma once
#include <boost/json.hpp>

#include "application/app.hpp"

namespace view
{
/**
 * @brief Converts application values to the public JSON format.
 * @details The map description catalog is copied from configuration once and then remains immutable.
 * It contains client metadata (models and colors) that domain algorithms do not need.
 * The view does not access mutable game state or the database.
 */
class JsonView
{
public:
    /** @brief Copies map descriptions from validated configuration without borrowing the source JSON. */
    explicit JsonView(const boost::json::value& config);
    /** @brief Returns an array of map identifiers and names. */
    boost::json::array Render(const app::GetMapsResult& result) const;
    /** @brief Returns a map description, including item appearance properties.
     * @throws std::out_of_range The identifier is absent from the description catalog.
     */
    boost::json::object Render(const app::GetMapDescResult& result) const;
    /** @brief Returns authToken and playerId for the new player session. */
    boost::json::object Render(const app::JoinGameResult& result) const;
    /** @brief Returns an object of player names indexed by string IDs. */
    boost::json::object Render(const app::GetPlayersListResult& result) const;
    /** @brief Serializes positions, velocities, bags, scores, and uncollected items. */
    boost::json::object Render(const app::GetGameStateResult& result) const;
    /** @brief Returns leaderboard records with play_time_ms converted to seconds. */
    boost::json::array Render(const app::GetGameRecordsResult& result) const;

private:
    /** @brief Immutable client map descriptions indexed by ID. */
    std::unordered_map<std::string, boost::json::object> m_map_descriptions;
};
} // namespace view
