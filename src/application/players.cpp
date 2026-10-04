#include <algorithm>

#include "application/players.h"
#include <limits>
#include <random>
#include <sstream>

namespace app {
using namespace std::literals;
const int kTokenSize = 32;

Token RandomToken::Generate() {

    std::random_device random_device_;
    std::mt19937_64 generator_(random_device_());

    const int kLowerBound = 0;
    const int kUpperBound = 15;
    std::uniform_int_distribution<> dist(kLowerBound, kUpperBound);
    std::stringstream ss;

    for (int i = 0; i < kTokenSize; i++) {
        ss << std::hex << dist(generator_);
    }

    return Token(ss.str());
}

Player::Player(Id id, Token token, GameSession* session, GameSession::DogPtr dog)
    : id_(id), token_(token), session_(session), dog_(dog) {}

const Player::Id& Player::GetId() const noexcept {
    return id_;
}

std::string Player::GetIdAsString() const {
    return std::to_string(*id_);
}

const Token& Player::GetToken() const {
    return token_;
}

std::string Player::GetName() const {
    return dog_->GetName();
}

GameSession* Player::GetSession() const {
    return session_;
}

GameSession::DogPtr Player::GetDog() const {
    return dog_;
}

void Players::AddPlayer(Player player) {
    const auto id = player.GetId();
    const auto token = player.GetToken();
    if (!player.GetSession() || !player.GetDog() ||
        player.GetSession()->FindDog(player.GetDog()->GetId()) != player.GetDog())
        throw std::invalid_argument("Player must reference a dog in its session");
    if (*id >= static_cast<unsigned>(std::numeric_limits<int>::max()))
        throw std::invalid_argument("Player id is outside the supported range");
    if (token_to_player_.contains(token) ||
        std::any_of(players_.begin(), players_.end(), [&](const auto& existing) {
            return existing->GetId() == id ||
                   (existing->GetSession() == player.GetSession() && existing->GetDog() == player.GetDog());
        }))
        throw std::invalid_argument("Duplicate player identity");
    auto next_player = std::make_shared<Player>(std::move(player));
    token_to_player_.emplace(token, next_player);
    try {
        players_.push_back(next_player);
    } catch (...) {
        token_to_player_.erase(token);
        throw;
    }
    next_player_id_ = std::max(next_player_id_, static_cast<int>(*id) + 1);
}

int Players::GetNextPlayerId() const noexcept {
    return next_player_id_;
}

Players::PlayerPtr Players::FindByToken(Token token) const {
    if (token_to_player_.count(token) > 0) {
        return token_to_player_.at(token);
    }
    return nullptr;
}

std::vector<Players::PlayerPtr> Players::GetSessionPlayersList(const GameSession& session) {
    std::vector<PlayerPtr> list;
    for (auto& player : players_) {
        const GameSession* next_session = player->GetSession();
        const GameSession::Id& next_session_id = next_session->GetId();
        const GameSession::Id& session_id = session.GetId();
        if (next_session_id == session_id) {
            list.push_back(player);
        }
    }
    return list;
}

const std::deque<Players::PlayerPtr>& Players::GetPlayersList() const {
    return players_;
}

void Players::RemovePlayer(GameSession::Id session_id, Dog::Id dog_id) {
    size_t idx = 0;
    for (const auto player : players_) {
        auto session = player->GetSession();
        auto dog = player->GetDog();
        if ((session->GetId() == session_id) && (dog->GetId() == dog_id)) {
            Token pl_token = players_.at(idx)->GetToken();
            players_.erase(players_.begin() + idx);
            token_to_player_.erase(pl_token);
            break;
        }
        idx++;
    }
}

} // namespace app
