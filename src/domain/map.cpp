#include "domain/model.h"
#include <algorithm>
#include <stdexcept>

namespace model {
using namespace std::literals;
Road::Road(HorizontalTag, Point start, Coord end_x) noexcept : start_{start}, end_{end_x, start.y} {
    start_x_range = static_cast<double>(std::min(start_.x, end_.x)) - width / 2.;
    end_x_range = static_cast<double>(std::max(start_.x, end_.x)) + width / 2.;
    start_y_range = static_cast<double>(start_.y) - width / 2.;
    end_y_range = static_cast<double>(end_.y) + width / 2.;
}

Road::Road(VerticalTag, Point start, Coord end_y) noexcept : start_{start}, end_{start.x, end_y} {
    start_x_range = static_cast<double>(start_.x) - width / 2.;
    end_x_range = static_cast<double>(end_.x) + width / 2.;
    start_y_range = static_cast<double>(std::min(start_.y, end_.y)) - width / 2.;
    end_y_range = static_cast<double>(std::max(start_.y, end_.y)) + width / 2.;
}

bool Road::IsHorizontal() const noexcept {
    return start_.y == end_.y;
}

bool Road::IsVertical() const noexcept {
    return start_.x == end_.x;
}

Point Road::GetStart() const noexcept {
    return start_;
}

Point Road::GetEnd() const noexcept {
    return end_;
}

bool Road::IsInArea(Position point) const {
    if ((point.x < start_x_range) || (point.x > end_x_range)) {
        return false;
    }

    if ((point.y < start_y_range) || (point.y > end_y_range)) {
        return false;
    }

    return true;
}

Position Road::GetNearestPos(Position target_pos) const {
    Position pos = target_pos;
    if (pos.x < start_x_range) {
        pos.x = start_x_range;
    } else if (pos.x > end_x_range) {
        pos.x = end_x_range;
    }

    if (pos.y < start_y_range) {
        pos.y = start_y_range;
    } else if (pos.y > end_y_range) {
        pos.y = end_y_range;
    }
    return pos;
}

Building::Building(Rectangle bounds) noexcept : bounds_{bounds} {}

const Rectangle& Building::GetBounds() const noexcept {
    return bounds_;
}

Office::Office(Id id, Point position, Offset offset) noexcept
    : id_{std::move(id)}, position_{position}, offset_{offset} {}

const Office::Id& Office::GetId() const noexcept {
    return id_;
}

Point Office::GetPosition() const noexcept {
    return position_;
}

Offset Office::GetOffset() const noexcept {
    return offset_;
}

Map::Map(Id id, std::string name) noexcept : id_(std::move(id)), name_(std::move(name)) {}

const Map::Id& Map::GetId() const noexcept {
    return id_;
}

const std::string& Map::GetName() const noexcept {
    return name_;
}

const Map::Buildings& Map::GetBuildings() const noexcept {
    return buildings_;
}

const Map::Roads& Map::GetRoads() const noexcept {
    return roads_;
}

const Map::Offices& Map::GetOffices() const noexcept {
    return offices_;
}

void Map::AddRoad(const Road& road) {
    roads_.emplace_back(road);
}

void Map::AddBuilding(const Building& building) {
    buildings_.emplace_back(building);
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (std::exception&) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

void Map::AddLoot(int type, LootProperties loot) {
    loot_props_[type] = std::move(loot);
}

std::optional<LootProperties> Map::GetLoot(int type) const noexcept {
    if (loot_props_.count(type) > 0) {
        return loot_props_.at(type);
    }
    return std::nullopt;
}

void Map::SetSpeed(double speed) {
    dog_speed_ = speed;
}

double Map::GetSpeed() const noexcept {
    return dog_speed_;
}

void Map::SetBagCapacity(int capacity) {
    bag_capacity_ = capacity;
}

int Map::GetBagCapacity() const noexcept {
    return bag_capacity_;
}

int Map::GetLootTypesNum() const noexcept {
    return loot_props_.size();
}

Position Map::GetRandomPoint() const {
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, static_cast<int>(roads_.size()) - 1);
    auto& random_road = roads_[dist(rd)];

    int x1 = std::min(random_road.GetStart().x, random_road.GetEnd().x);
    int y1 = std::min(random_road.GetStart().y, random_road.GetEnd().y);
    int x2 = std::max(random_road.GetStart().x, random_road.GetEnd().x);
    int y2 = std::max(random_road.GetStart().y, random_road.GetEnd().y);

    std::uniform_int_distribution<int> dist_x(x1, x2);
    std::uniform_int_distribution<int> dist_y(y1, y2);

    double x_random = static_cast<double>(dist_x(rd));
    double y_random = static_cast<double>(dist_y(rd));

    return {x_random, y_random};
}

} // namespace model
