#include "domain/model.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace model {
using namespace std::literals;
Dog::Dog(Id id, std::string name, Position spawn_point, double abs_speed, std::string direction,
         int bag_capacity, int score)
    : id_(id), name_(std::move(name)), position_(spawn_point), abs_speed_(abs_speed), map_speed_({0, 0}),
      dir_(direction), bag_capacity_(bag_capacity), score_(score) {}

const Dog::Id& Dog::GetId() const noexcept {
    return id_;
}

const std::string& Dog::GetName() const noexcept {
    return name_;
}

void Dog::SetPosition(Position position) {
    position_ = position;
}

Position Dog::GetPosition() const noexcept {
    return position_;
}

geom::Vec2D Dog::GetSpeed() const noexcept {
    return map_speed_;
}

double Dog::GetAbsSpeed() const noexcept {
    return abs_speed_;
}

void Dog::SetDirection(std::string dir) {
    using namespace std::literals;
    if (dir == "U"s) {
        dir_ = dir;
        map_speed_ = {0., -abs_speed_};
    } else if (dir == "D"s) {
        dir_ = dir;
        map_speed_ = {0., abs_speed_};
    } else if (dir == "L"s) {
        dir_ = dir;
        map_speed_ = {-abs_speed_, 0.};
    } else if (dir == "R"s) {
        dir_ = dir;
        map_speed_ = {abs_speed_, 0.};
    } else if (dir.empty()) {
        map_speed_ = {0., 0.};
    } else {
        throw std::runtime_error("Invalid direction");
    }
}

std::string Dog::GetDirection() const noexcept {
    return dir_;
}

void Dog::PutLootInBag(Loot loot) {
    if (loot_list_.size() < bag_capacity_) {
        loot_list_.push_back(std::move(loot));
    }
}

void Dog::ExchangeLoot() {
    for (const auto& loot : loot_list_) {
        score_ += loot.GetProps().value;
    }
    loot_list_.clear();
}

std::vector<Loot> Dog::GetLootList() const {
    return loot_list_;
}

int Dog::GetBagCapacity() const noexcept {
    return bag_capacity_;
}

int Dog::GetScore() const noexcept {
    return score_;
}

int Dog::GetPlayTime() const noexcept {
    return play_time_;
}

int Dog::GetInactivityTime() const noexcept {
    return inactivity_time_;
}

void Dog::ValidateTimeDelta(int time_delta) const {
    if (time_delta < 0 || play_time_ > std::numeric_limits<int>::max() - time_delta) {
        throw InvalidTimeDelta("Time delta is outside the supported range");
    }
}

void Dog::UpdateState(int time_delta) {
    ValidateTimeDelta(time_delta);
    play_time_ += time_delta;

    if ((map_speed_.x != 0) || (map_speed_.y != 0)) {
        inactivity_time_ = 0;
    } else {
        inactivity_time_ += time_delta;
    }
}

bool Dog::IsValidDirection(std::string dir) {
    using namespace std::literals;
    if ((dir == "U"s) || (dir == "D"s) || (dir == "L"s) || (dir == "R"s) || dir.empty()) {
        return true;
    }
    return false;
}

Loot::Loot(Id id, LootProperties loot_prop, Position pos)
    : id_(id), loot_prop_(std::move(loot_prop)), pos_(pos) {}

const Loot::Id& Loot::GetId() const noexcept {
    return id_;
}

LootProperties Loot::GetProps() const noexcept {
    return loot_prop_;
}

Position Loot::GetPos() const noexcept {
    return pos_;
}

} // namespace model
