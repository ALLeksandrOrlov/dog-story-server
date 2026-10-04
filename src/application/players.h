#pragma once
#include "common/tagged.h"
#include "domain/model.h"
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace detail {
struct TokenTag {};
} // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

namespace app {
using model::Dog;
using model::GameSession;

class RandomToken {
  public:
    RandomToken() = delete;

    static Token Generate();
};

class Player {
  public:
    using Id = util::Tagged<std::uint32_t, Player>;

    Player(Id id, Token token, GameSession* session, GameSession::DogPtr dog);

    const Id& GetId() const noexcept;
    std::string GetIdAsString() const;

    const Token& GetToken() const;

    std::string GetName() const;

    GameSession* GetSession() const;

    GameSession::DogPtr GetDog() const;

  private:
    Id id_{0};
    Token token_{""};
    GameSession* session_;
    GameSession::DogPtr dog_;
};

class Players {
  public:
    using PlayerPtr = std::shared_ptr<Player>;

    void AddPlayer(Player player);
    int GetNextPlayerId() const noexcept;

    PlayerPtr FindByToken(Token token) const;

    std::vector<PlayerPtr> GetSessionPlayersList(const GameSession& session);
    const std::deque<PlayerPtr>& GetPlayersList() const;

    void RemovePlayer(GameSession::Id session_id, Dog::Id dog_id);

  private:
    using TokenIdHasher = util::TaggedHasher<Token>;
    using TokenToPlayer = std::unordered_map<Token, PlayerPtr, TokenIdHasher>;

    std::deque<PlayerPtr> players_;
    TokenToPlayer token_to_player_;

    int next_player_id_ = 0;
};

} // namespace app
