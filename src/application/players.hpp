#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/tagged.hpp"
#include "domain/game_session.hpp"

namespace app
{
namespace detail
{
/** @brief Distinguishes access tokens from ordinary strings at compile time. */
struct TokenTag
{
};
} // namespace detail

/** @brief Authorization secret; a distinct type prevents mixing it with ordinary strings. */
using Token = util::Tagged<std::string, detail::TokenTag>;

using model::Dog;
using model::GameSession;

/** @brief Creates access tokens directly from the operating system's random source.
 * @details On supported Linux/macOS systems, std::random_device uses the OS source.
 * No intermediate PRNG with a short seed is used.
 */
class RandomToken
{
public:
    /** @brief Prevents instantiation of this stateless utility. */
    RandomToken() = delete;
    /** @brief Returns 32 hexadecimal characters; propagates random source failures to the caller. */
    static Token Generate();
};

/** @brief Associates player authorization with a dog and a session.
 * @details Retains the dog through shared_ptr but does not own the session. The session
 * must outlive the player; the Players registry validates the dog's session membership.
 */
class Player
{
public:
    /** @brief Strongly typed entity identifier. */
    using Id = util::Tagged<std::uint32_t, Player>;
    /** @brief Creates a record for subsequent validated registration in Players. */
    Player(const Id id, const Token& token, GameSession* session, const GameSession::DogPtr& dog);
    /** @brief Returns the player's identifier, unique across the game. */
    const Id& GetId() const noexcept;
    /** @brief Converts the identifier to a decimal response key. */
    std::string GetIdAsString() const;
    /** @brief Returns the authorization secret, valid until registration is removed. */
    const Token& GetToken() const;
    /** @brief Returns the associated dog's name. */
    std::string GetName() const;
    /** @brief Returns a borrowed pointer to the player's session. */
    GameSession* GetSession() const;
    /** @brief Returns shared ownership of the player's dog. */
    GameSession::DogPtr GetDog() const;

private:
    /** @brief Entity identifier that does not change after registration. */
    Id m_id{0};
    /** @brief Access token for the associated player. */
    Token m_token{std::string{}};
    /** @brief Borrowed session pointer; Game must outlive the player. */
    GameSession* m_session = nullptr;
    /** @brief Dog controlled by the authorized player. */
    GameSession::DogPtr m_dog{};
};

/** @brief Registry of player authorization and associations with the domain world.
 * @details Access is serialized with game actions. Copying is prohibited because
 * independent registries must not share mutable players.
 */
class Players
{
public:
    /** @brief Shared ownership of a registered player. */
    using PlayerPtr = std::shared_ptr<Player>;
    /** @brief Creates an empty registry. */
    Players() = default;
    /** @brief Prevents implicit sharing of players between registry copies. */
    Players(const Players&) = delete;
    /** @brief Prevents copy assignment of the registry. */
    Players& operator=(const Players&) = delete;
    /** @brief Moves the registry while preserving session associations. */
    Players(Players&&) = default;
    /** @brief Moves the registry; the destination takes over existing world associations. */
    Players& operator=(Players&&) = default;
    /** @brief Atomically adds a player to all indexes.
     * @throws std::invalid_argument The ID, token, or dog is duplicated, or the session association is invalid.
     */
    void AddPlayer(const Player& player);
    /** @brief Returns the next monotonic identifier without reserving it. */
    int GetNextPlayerId() const noexcept;
    /** @brief Finds an authorized player; nullptr indicates an unknown or revoked token. */
    PlayerPtr FindByToken(const Token& token) const;
    /** @brief Builds a player list for the specified session only. */
    std::vector<PlayerPtr> GetSessionPlayersList(const GameSession& session) const;
    /** @brief Exposes the registry for snapshots without allowing structural changes. */
    const std::deque<PlayerPtr>& GetPlayersList() const;
    /** @brief Removes the dog's association and revokes the token; a missing entry is allowed. */
    void RemovePlayer(const GameSession::Id session_id, const Dog::Id dog_id);

private:
    /** @brief Hash function for typed tokens in the authorization index. */
    using TokenIdHasher = util::TaggedHasher<Token>;
    /** @brief Authorization index mapping tokens to registered players. */
    using TokenToPlayer = std::unordered_map<Token, PlayerPtr, TokenIdHasher>;
    /** @brief Registry of registered players. */
    std::deque<PlayerPtr> m_players{};
    /** @brief Index of active authorization tokens. */
    TokenToPlayer m_token_to_player{};
    /** @brief Next player ID; removed IDs are not reused. */
    int m_next_player_id = 0;
};
} // namespace app
