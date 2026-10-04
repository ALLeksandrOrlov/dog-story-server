#include "domain/collision_detector.h"
#include "domain/loot_generator.h"
#include "domain/model.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace model {
using namespace std::literals;
using namespace collision_detector;
namespace {
class VectorItemGathererProvider : public collision_detector::ItemGathererProvider {
  public:
    VectorItemGathererProvider(std::vector<collision_detector::Item> items,
                               std::vector<collision_detector::Gatherer> gatherers)
        : items_(items), gatherers_(gatherers) {}

    size_t ItemsCount() const override {
        return items_.size();
    }

    collision_detector::Item GetItem(size_t idx) const override {
        return items_.at(idx);
    }

    size_t GatherersCount() const override {
        return gatherers_.size();
    }

    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return gatherers_[idx];
    }

  private:
    std::vector<collision_detector::Item> items_;
    std::vector<collision_detector::Gatherer> gatherers_;
};

} // namespace
GameSession::GameSession(Id id, const Map* map, LootGeneratorConfig loot_generator_config,
                         int dog_retirement_time)
    : id_(id), map_(map), loot_generator_config_(loot_generator_config),
      dog_retirement_time_(dog_retirement_time),
      loot_generator_(std::chrono::duration_cast<loot_gen::LootGenerator::TimeInterval>(
                          std::chrono::duration<double>(loot_generator_config.period)),
                      loot_generator_config.probability) {
    if (!map_ || map_->GetRoads().empty())
        throw std::invalid_argument("Session requires a map with roads");
}

const GameSession::Id& GameSession::GetId() const noexcept {
    return id_;
}

void GameSession::AddDog(Dog dog) {
    const auto id = dog.GetId();
    if (dog_id_to_dog_.contains(id))
        throw std::invalid_argument("Duplicate dog id");
    if (*id >= static_cast<unsigned>(std::numeric_limits<int>::max()))
        throw std::invalid_argument("Dog id is outside the supported range");
    auto next_dog = std::make_shared<Dog>(std::move(dog));
    dog_id_to_dog_.emplace(id, next_dog);
    try {
        dogs_.push_back(next_dog);
    } catch (...) {
        dog_id_to_dog_.erase(id);
        throw;
    }
    next_dog_id_ = std::max(next_dog_id_, static_cast<int>(*id) + 1);
}

const GameSession::Dogs& GameSession::GetDogs() const {
    return dogs_;
}

GameSession::DogPtr GameSession::FindDog(const Dog::Id& id) const {
    if (dog_id_to_dog_.count(id) > 0) {
        return dog_id_to_dog_.at(id);
    }
    return nullptr;
}

int GameSession::GetNextDogId() const noexcept {
    return next_dog_id_;
}

void GameSession::AddLoot(Loot loot) {
    const auto id = loot.GetId();
    if (std::any_of(loot_list_.begin(), loot_list_.end(),
                    [id](const auto& item) { return item.GetId() == id; }))
        throw std::invalid_argument("Duplicate loot id");
    if (*id >= static_cast<unsigned>(std::numeric_limits<int>::max()))
        throw std::invalid_argument("Loot id is outside the supported range");
    loot_list_.push_back(std::move(loot));
    next_loot_id_ = std::max(next_loot_id_, static_cast<int>(*id) + 1);
}

GameSession::Loots GameSession::GetLoots() const {
    return loot_list_;
}

int GameSession::GetNextLootId() const noexcept {
    return next_loot_id_;
}

const Map* GameSession::GetMap() const {
    return map_;
}

void GameSession::SetDogRetirementHandler(std::function<void(Id, Dog::Id)> on_dog_retirement) {
    on_dog_retirement_ = on_dog_retirement;
}

void GameSession::UpdateState(int time_delta) {
    for (const auto& dog : dogs_)
        dog->ValidateTimeDelta(time_delta);
    UpdateDogsState(time_delta);
    UpdateCollisions(time_delta);
    UpdateLoot(time_delta);
}

std::pair<double, Position> GameSession::GetCloserToPoint(Position current_pos, Position target_pos) const {
    const Map::Roads& roads = map_->GetRoads();
    std::vector<std::pair<double, Position>> distances_to_target;
    for (const auto& road : roads) {
        if (road.IsInArea(current_pos)) {
            Position next_point = road.GetNearestPos(target_pos);
            double distance = sqrt(pow(next_point.x - target_pos.x, 2) + pow(next_point.y - target_pos.y, 2));
            bool is_target_reached = !(distance > 0.);
            if (is_target_reached) {
                distances_to_target.clear();
                distances_to_target.push_back({distance, next_point});
                break;
            }
            distances_to_target.push_back({distance, next_point});
        }
    }

    if (distances_to_target.empty())
        throw std::logic_error("Player position is outside map roads");
    return *std::min_element(distances_to_target.begin(), distances_to_target.end(),
                             [](const std::pair<double, Position>& a, const std::pair<double, Position>& b) {
                                 return a.first < b.first;
                             });
}

void GameSession::RemoveDog(size_t index) {
    auto dog_id = dogs_.at(index)->GetId();
    dogs_.erase(dogs_.begin() + index);
    dog_id_to_dog_.erase(dog_id);
}

int GameSession::GetRandomLootType() {
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, map_->GetLootTypesNum() - 1);
    return dist(rd);
}

void GameSession::RemoveLoot(size_t index) {
    loot_list_.erase(loot_list_.begin() + index);
}

void GameSession::UpdateDogsState(int time_delta) {
    size_t idx = 0;
    std::vector<size_t> retired_dog_idxes;
    for (auto& dog : dogs_) {
        dog->UpdateState(time_delta);
        if (dog->GetInactivityTime() >= dog_retirement_time_) {
            retired_dog_idxes.push_back(idx);
        }
        idx++;
    }

    for (auto it = retired_dog_idxes.rbegin(); it != retired_dog_idxes.rend(); ++it) {
        const auto index = *it;
        if (on_dog_retirement_) {
            on_dog_retirement_(id_, dogs_.at(index)->GetId());
        } else {
            throw std::runtime_error("on_dog_retirement_ not set");
        }

        RemoveDog(index);
    }
}

void GameSession::UpdateCollisions(int time_delta) {
    using namespace std::literals;

    std::vector<Gatherer> gatherers;
    for (auto& dog : dogs_) {
        Position pos = dog->GetPosition();
        geom::Vec2D speed = dog->GetSpeed();
        const double MSEC_IN_SEC = 1000.;
        Position target = {(pos.x + speed.x * (time_delta / MSEC_IN_SEC)),
                           (pos.y + speed.y * (time_delta / MSEC_IN_SEC))};

        auto distance_pos = GetCloserToPoint(pos, target);
        if (distance_pos.first > 0.) {
            distance_pos = GetCloserToPoint(distance_pos.second, target);
        }

        if (distance_pos.first > 0.) {
            dog->SetDirection(""s);
        }
        dog->SetPosition(distance_pos.second);

        const double DOG_WIDTH = 0.6;
        gatherers.push_back({{pos.x, pos.y}, {distance_pos.second.x, distance_pos.second.y}, DOG_WIDTH});
    }

    std::vector<Item> items;
    for (const auto& loot : loot_list_) {
        Position pos = loot.GetPos();
        items.push_back({{pos.x, pos.y}, 0.});
    }
    for (const auto& office : map_->GetOffices()) {
        Point pos = office.GetPosition();
        const double OFFICE_WIDTH = 0.5;
        items.push_back({{static_cast<double>(pos.x), static_cast<double>(pos.y)}, OFFICE_WIDTH});
    }

    VectorItemGathererProvider provider(std::move(items), std::move(gatherers));
    std::vector<GatheringEvent> events(FindGatherEvents(provider));

    std::unordered_map<size_t, size_t> item_to_gather;
    std::vector<size_t> loot_event_idxes;
    for (const auto& event : events) {
        auto dog = dogs_[event.gatherer_id];
        if (event.item_id < loot_list_.size()) {
            if ((dog->GetLootList().size() < dog->GetBagCapacity()) &&
                (item_to_gather.count(event.item_id) == 0)) {
                item_to_gather[event.item_id] = event.gatherer_id;
                dog->PutLootInBag(loot_list_[event.item_id]);
                loot_event_idxes.push_back(event.item_id);
            }
        } else {
            dog->ExchangeLoot();
        }
    }

    // remove from map
    std::sort(loot_event_idxes.rbegin(), loot_event_idxes.rend());
    for (size_t idx : loot_event_idxes) {
        RemoveLoot(idx);
    }
}

void GameSession::UpdateLoot(int time_delta) {
    using namespace loot_gen;
    if (map_->GetLootTypesNum() == 0)
        return;
    unsigned new_loots_num = loot_generator_.Generate(LootGenerator::TimeInterval(time_delta),
                                                      static_cast<unsigned>(loot_list_.size()),
                                                      static_cast<unsigned>(dogs_.size()));
    for (; new_loots_num > 0; new_loots_num--) {
        int random_loot_type = GetRandomLootType();
        Position random_point = map_->GetRandomPoint();
        auto loot_id = Loot::Id(GetNextLootId());
        auto loot_opt = map_->GetLoot(random_loot_type);
        AddLoot(Loot(loot_id, *loot_opt, random_point));
    }
}

} // namespace model
