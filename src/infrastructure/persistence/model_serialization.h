#pragma once

#include "domain/model.h"
#include <algorithm>
#include <boost/serialization/vector.hpp>

namespace model {

template <typename Archive>
void serialize(Archive& ar, LootProperties& loot_props, [[maybe_unused]] const unsigned version) {
    ar & loot_props.type;
    ar & loot_props.value;
}

template <typename Archive>
void serialize(Archive& ar, Position& position, [[maybe_unused]] const unsigned version) {
    ar & position.x;
    ar & position.y;
}

} // namespace model

namespace serialization {
using namespace std::literals;

class LootRepr {
  public:
    LootRepr() = default;

    LootRepr(const model::Loot& loot) : id_(loot.GetId()), loot_prop_(loot.GetProps()), pos_(loot.GetPos()) {}

    model::Loot Restore() const {
        return {id_, loot_prop_, pos_};
    }

    template <typename Archive> void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar&* id_;
        ar & loot_prop_;
        ar & pos_;
    }

  private:
    model::Loot::Id id_{0};
    model::LootProperties loot_prop_;
    model::Position pos_;
};

class DogRepr {
  public:
    DogRepr() = default;

    DogRepr(const model::Dog& dog)
        : id_(dog.GetId()), name_(dog.GetName()), spawn_point_(dog.GetPosition()),
          abs_speed_(dog.GetAbsSpeed()), direction_(dog.GetDirection()), bag_capacity_(dog.GetBagCapacity()),
          score_(dog.GetScore()) {
        auto loot_list = dog.GetLootList();
        for (const auto& loot : loot_list) {
            loot_list_.emplace_back(loot);
        }
    }

    model::Dog Restore() const {
        model::Dog dog(id_, name_, spawn_point_, abs_speed_, direction_, bag_capacity_, score_);
        for (const auto& loot : loot_list_) {
            dog.PutLootInBag(loot.Restore());
        }
        return dog;
    }

    template <typename Archive> void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar&* id_;
        ar & name_;
        ar & spawn_point_;
        ar & abs_speed_;
        ar & direction_;
        ar & bag_capacity_;
        ar & score_;
        ar & loot_list_;
    }

  private:
    model::Dog::Id id_{0};
    std::string name_;
    model::Position spawn_point_;
    double abs_speed_;
    std::string direction_;
    int bag_capacity_;
    int score_;
    std::vector<LootRepr> loot_list_;
};

class GameSessionRepr {
  public:
    GameSessionRepr() = default;

    GameSessionRepr(const model::GameSession& session)
        : id_(session.GetId()), map_id_(session.GetMap()->GetId()) {
        auto dogs = session.GetDogs();
        for (auto dog : dogs) {
            dogs_.emplace_back(*dog);
        }

        auto loots = session.GetLoots();
        for (const auto& loot : loots) {
            loots_.emplace_back(loot);
        }
    }

    model::GameSession Restore(const model::Game& game) const {
        const model::Map* map = game.FindMap(map_id_);
        if (!map)
            throw std::runtime_error("Snapshot session references an unknown map");
        model::GameSession session(id_, map, game.GetLootGeneratorConfig(), game.GetDogRetirementTime());

        for (const auto& dog : dogs_) {
            auto restored = dog.Restore();
            const auto position = restored.GetPosition();
            const auto& roads = map->GetRoads();
            if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
                std::none_of(roads.begin(), roads.end(),
                             [position](const auto& road) { return road.IsInArea(position); }))
                throw std::runtime_error("Snapshot player position is outside map roads");
            session.AddDog(std::move(restored));
        }

        for (const auto& loot : loots_) {
            session.AddLoot(loot.Restore());
        }
        return session;
    }

    template <typename Archive> void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar&* id_;
        ar&* map_id_;
        ar & dogs_;
        ar & loots_;
    }

  private:
    model::GameSession::Id id_{0};
    model::Map::Id map_id_{""s};
    std::vector<DogRepr> dogs_;
    std::vector<LootRepr> loots_;
};

} // namespace serialization