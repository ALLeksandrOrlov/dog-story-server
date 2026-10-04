#pragma once

#include <boost/serialization/vector.hpp>

#include "application/app.h"
#include "domain/model.h"

namespace serialization {

class PlayerRepr {
  public:
    PlayerRepr() = default;

    PlayerRepr(const app::Player& player)
        : id_(player.GetId()), token_(player.GetToken()), map_id_(player.GetSession()->GetMap()->GetId()),
          dog_id_(player.GetDog()->GetId()) {}

    app::Player Restore(const model::Game& game) const {
        model::GameSession* session = game.FindSession(map_id_);
        if (!session)
            throw std::runtime_error("Snapshot player references an unknown session");
        auto dog = session->FindDog(dog_id_);
        if (!dog)
            throw std::runtime_error("Snapshot player references an unknown dog");
        return {id_, token_, session, std::move(dog)};
    }

    template <typename Archive> void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar&* id_;
        ar&* token_;
        ar&* map_id_;
        ar&* dog_id_;
    }

  private:
    app::Player::Id id_{0};
    Token token_{""};
    model::Map::Id map_id_{""};
    model::Dog::Id dog_id_{0};
};

class PlayersRepr {
  public:
    PlayersRepr() = default;

    PlayersRepr(const app::Players& players) {
        auto players_list = players.GetPlayersList();
        for (auto player : players_list) {
            players_.emplace_back(*player);
        }
    }

    app::Players Restore(const model::Game& game) {
        app::Players players;
        for (const auto& player : players_) {
            players.AddPlayer(player.Restore(game));
        }
        return players;
    }

    template <typename Archive> void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & players_;
    }

  private:
    std::vector<PlayerRepr> players_;
};

} // namespace serialization