#pragma once

#include <deque>
#include <unordered_map>

#include "domain/game_session.hpp"
#include "domain/map.hpp"

namespace model
{
/** @brief Root aggregate that owns maps, sessions, and lookup indexes.
 * @details Map and session addresses remain stable on insertion. Copying is disabled;
 * moving is allowed before external handlers are bound to a specific Game.
 */
class Game
{
public:
    /** @brief Map catalog with stable element addresses. */
    using Maps = std::deque<Map>;
    /** @brief Sessions with stable element addresses. */
    using GameSessions = std::deque<GameSession>;
    /** @brief Creates an empty game with default settings. */
    Game() = default;
    /** @brief Disables copying of the interconnected object graph. */
    Game(const Game&) = delete;
    /** @brief Disables copying of the game session owner. */
    Game& operator=(const Game&) = delete;
    /** @brief Moves the aggregate while preserving element addresses. */
    Game(Game&&) = default;
    /** @brief Moves the aggregate; the destination's external dependencies must be rebound. */
    Game& operator=(Game&&) = default;
    /** @brief Adds a configuration with a unique ID; existing addresses remain unchanged. */
    void AddMap(const Map& map);
    /** @brief Returns the map catalog for read access only. */
    const Maps& GetMaps() const noexcept;
    /** @brief Finds a map by ID; returns nullptr if absent. */
    const Map* FindMap(const Map::Id& id) const noexcept;
    /** @brief Takes ownership of a session: one per map, with the map belonging to this game. */
    void AddSession(GameSession session);
    /** @brief Returns sessions without allowing callers to bypass internal index maintenance. */
    const GameSessions& GetSessions() const;
    /** @brief Finds the map's mutable session; nullptr means there is no active world. */
    GameSession* FindSession(const Map::Id& id) const noexcept;
    /** @brief Finds a mutable session by its identifier. */
    GameSession* FindSession(const GameSession::Id& id) const noexcept;
    /** @brief Returns the next monotonically increasing session ID without reserving it. */
    int GetNextSessionId() const noexcept;
    /** @brief Sets validated options for new sessions; existing sessions remain unchanged. */
    void SetLootGeneratorConfig(const LootGeneratorConfig config);
    /** @brief Returns the generation settings used to create a session. */
    const LootGeneratorConfig& GetLootGeneratorConfig() const;
    /** @brief Sets a positive idle timeout for new sessions, in milliseconds. */
    void SetDogRetirementTime(const int time);
    /** @brief Returns the idle timeout in milliseconds. */
    int GetDogRetirementTime() const;
    /** @brief Validates the step in every session, then updates the worlds sequentially.
     * @throws InvalidTimeDelta No session changes if the time step is invalid.
     * @details Errors in the external retirement handler do not trigger a global rollback.
     */
    void UpdateState(const int time_delta);

private:
    /** @brief Hash function for strongly typed map identifiers. */
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    /** @brief Hash function for strongly typed session identifiers. */
    using SessionIdHasher = util::TaggedHasher<GameSession::Id>;
    /** @brief Maps a map identifier to its catalog index. */
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;
    /** @brief Non-owning index of the single session for each map. */
    using MapIdToSession = std::unordered_map<Map::Id, GameSession*, MapIdHasher>;
    /** @brief Non-owning session index keyed by session identifier. */
    using SessionIdToSession = std::unordered_map<GameSession::Id, GameSession*, SessionIdHasher>;
    /** @brief Owning storage for maps that are immutable after insertion. */
    Maps m_maps{};
    /** @brief Map index keyed by unique identifier. */
    MapIdToIndex m_map_id_to_index{};
    /** @brief Owning storage for game sessions. */
    GameSessions m_sessions{};
    /** @brief Index of the active world for each map. */
    MapIdToSession m_map_id_to_session{};
    /** @brief Session index keyed by unique identifier. */
    SessionIdToSession m_session_id_to_session{};
    /** @brief Next session ID; existing identifiers are not reused. */
    int m_next_session_id = 0;
    /** @brief Generator settings for new sessions. */
    LootGeneratorConfig m_loot_generator_config{};
    /** @brief Continuous idle timeout before retirement, in milliseconds. */
    int m_dog_retirement_time = 60000;
};
} // namespace model
