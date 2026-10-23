#pragma once

#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "domain/game.hpp"
#include "domain/retired_player.hpp"

#include "application/players.hpp"

namespace app
{
using model::Game;
using model::Map;

/** @brief Source of simulation steps: a test HTTP request or an internal timer. */
enum class ApplicationMode
{
    Manual,
    Auto
};

/** @brief Reason for rejecting an operation, independent of HTTP status codes and JSON. */
enum class ApplicationErrorReason
{
    InvalidName,
    InvalidMap,
    InvalidToken,
    InvalidDirection,
    InvalidState,
    InvalidTimeDelta,
    InvalidMaxItems
};

/** @brief Application error; the controller maps its reason to a public API response. */
class ApplicationError : public std::exception
{
public:
    /** @brief Stores a reason and diagnostic message without transport dependencies. */
    ApplicationError(const ApplicationErrorReason reason, const std::string& message):
            m_reason(reason),
            m_message(message)
    {
    }
    /** @brief Returns the diagnostic message required by std::exception. */
    const char* what() const noexcept override
    {
        return m_message.c_str();
    }
    /** @brief Returns a stable reason that can be mapped to a transport error. */
    ApplicationErrorReason GetReason() const noexcept
    {
        return m_reason;
    }

private:
    /** @brief Reason for rejecting the operation, used for transport error mapping. */
    ApplicationErrorReason m_reason;
    /** @brief Exception message exposed through the standard what() method. */
    std::string m_message{};
};

/** @brief Summary of a map in the catalog. */
struct MapSummary
{
    /** @brief Public identifier of the entity. */
    std::string id{};
    /** @brief Display name of the entity. */
    std::string name{};
};
/** @brief Result of reading the available map catalog. */
struct GetMapsResult
{
    /** @brief Summaries of the available maps. */
    std::vector<MapSummary> maps{};
};
/** @brief Validated map identifier used to present its configuration. */
struct GetMapDescResult
{
    /** @brief Public identifier of the entity. */
    model::Map::Id id{std::string{}};
};
/** @brief Registration details of a player who has successfully joined. */
struct JoinGameResult
{
    /** @brief Authorization secret for the new player. */
    Token token{std::string{}};
    /** @brief Identifier of the successfully registered player. */
    Player::Id player_id{0};
};
/** @brief Public player identity without an authorization token. */
struct PlayerSummary
{
    /** @brief Public identifier of the entity. */
    Player::Id id{0};
    /** @brief Display name of the entity. */
    std::string name{};
};
/** @brief Players in one session visible to an authorized participant. */
struct GetPlayersListResult
{
    /** @brief Player snapshots for the current session. */
    std::vector<PlayerSummary> players{};
};
/** @brief Snapshot of one player's state for response serialization. */
struct PlayerState
{
    /** @brief Public identifier of the entity. */
    Player::Id id{0};
    /** @brief Position in map coordinates. */
    model::Position position{};
    /** @brief Velocity vector in map units per second. */
    geom::Vec2D speed{};
    /** @brief Orientation: U/D/L/R. */
    std::string direction{};
    /** @brief Items carried by the player. */
    std::vector<model::Loot> bag{};
    /** @brief Nonnegative score earned by delivering items. */
    int score = 0;
};
/** @brief Independent snapshot of players and items in one game session. */
struct GetGameStateResult
{
    /** @brief Player snapshots for the current session. */
    std::vector<PlayerState> players{};
    /** @brief Items on the map that have not been collected. */
    model::GameSession::Loots lost_objects{};
};
/** @brief Ordered page of stored game results. */
struct GetGameRecordsResult
{
    /** @brief Completed game results in leaderboard order. */
    std::vector<domain::RetiredPlayer> records{};
};

/** @brief Coordinates game operations, authorization, and the result persistence port.
 * @details Has no knowledge of HTTP, JSON, or the database implementation. All calls run
 * sequentially on one executor. Game, Players, and Repository must outlive
 * Application and its registered player retirement handlers.
 */
class Application
{
public:
    /** @brief Injectable source of authorization tokens, independent of transport. */
    using TokenGenerator = std::function<Token()>;
    /** @brief Binds handlers to existing sessions and stores the injected dependencies.
     * @param game Domain world; keeps maps and sessions at stable addresses.
     * @param players Player registry associated with this world.
     * @param mode Allowed source of simulation ticks.
     * @param is_random_spawn_point Chooses a random position instead of the start of the first road.
     * @param players_repo Port for storing completed game results.
     * @param on_update Called after a successful tick; used to save snapshots.
     * @param token_generator Secret generator that can be substituted in tests.
     */
    Application(Game& game, Players& players, const ApplicationMode mode, const bool is_random_spawn_point,
            const std::function<void(int)>& on_update, domain::RetiredPlayerRepository& players_repo,
            const TokenGenerator& token_generator = RandomToken::Generate);
    /** @brief Prevents copying a coordinator of external dependencies. */
    Application(const Application&) = delete;
    /** @brief Prevents replacing dependencies through copy assignment. */
    Application& operator=(const Application&) = delete;
    /** @brief Returns the map catalog without exposing mutable domain objects. */
    GetMapsResult GetMaps() const;
    /** @brief Checks that a map exists and returns its identifier for the view. */
    GetMapDescResult GetMapDesc(const Map::Id& map_id) const;
    /** @brief Creates a player and dog, issuing a unique authorization secret.
     * @param map_id Identifier of an existing map.
     * @param user_name Nonempty name without NUL; ordinary Unicode characters and long names are allowed.
     * @throws ApplicationError The name is empty, contains NUL, or the map does not exist.
     * @details Failed registration leaves no dog without a player; the consumed ID is not reused.
     */
    JoinGameResult JoinGame(const Map::Id& map_id, const std::string_view user_name);
    /** @brief Returns only players from the token owner's session. */
    GetPlayersListResult GetPlayersList(const Token& token) const;
    /** @brief Returns a snapshot of only the token owner's session. */
    GetGameStateResult GetGameState(const Token& token) const;
    /** @brief Checks authorization and applies U/D/L/R movement or a stop command. */
    void DoAction(const Token& token, const std::string& direction);
    /** @brief Advances manual simulation; disallowed when the automatic timer is enabled. */
    void ManualTick(const int time_delta);
    /** @brief Advances automatic simulation; disallowed in manual mode. */
    void AutoTick(const int time_delta);
    /** @brief Reads a page of results: offset >= 0 and page size from 0 to 100. */
    GetGameRecordsResult GetGameRecords(const std::optional<int> start, const std::optional<int> max_items);
    /** @brief Checks token syntax: exactly 32 hexadecimal characters. */
    static bool IsValidToken(const Token& token);

private:
    /** @brief Borrowed reference to the domain world; outlives session handlers. */
    Game& m_game;
    /** @brief Registry of registered players. */
    Players& m_players;
    /** @brief Allowed source of simulation ticks. */
    ApplicationMode m_mode = ApplicationMode::Manual;
    /** @brief Whether to choose a random spawn point on a road. */
    bool m_is_random_spawn_point = false;
    /** @brief Successful tick handler used for periodic snapshots. */
    std::function<void(int)> m_on_update{};
    /** @brief Borrowed reference to the port for storing completed game results. */
    domain::RetiredPlayerRepository& m_players_repo;
    /** @brief Secret source that can be substituted to test registration failures. */
    TokenGenerator m_token_generator{};

    /** @brief Checks the tick source and maps domain time errors to application errors. */
    void Tick(const int time_delta, const ApplicationMode expected_mode);
    /** @brief Returns an authorized player or throws ApplicationError. */
    Players::PlayerPtr RequirePlayer(const Token& token) const;
    /** @brief Registers result persistence before removal of the player and token. */
    void BindSession(GameSession& session);
    /** @brief Finds an unused token; limits retries if the generator is faulty. */
    Token GenerateUniqueToken() const;
};
} // namespace app
