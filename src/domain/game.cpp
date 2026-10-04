#include "domain/model.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace model {
using namespace std::literals;
void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (std::exception&) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

const Game::Maps& Game::GetMaps() const noexcept {
    return maps_;
}

const Map* Game::FindMap(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
        return &maps_.at(it->second);
    }
    return nullptr;
}

void Game::AddSession(GameSession session) {
    const auto id = session.GetId();
    const auto* map = session.GetMap();
    if (!map || FindMap(map->GetId()) != map)
        throw std::invalid_argument("Session map must belong to this game");
    if (session_id_to_session_.contains(id) || map_id_to_session_.contains(map->GetId()))
        throw std::invalid_argument("Duplicate session or session map");
    if (*id >= static_cast<unsigned>(std::numeric_limits<int>::max()))
        throw std::invalid_argument("Session id is outside the supported range");
    sessions_.push_back(std::move(session));
    try {
        map_id_to_session_.emplace(map->GetId(), &sessions_.back());
        session_id_to_session_.emplace(id, &sessions_.back());
    } catch (...) {
        map_id_to_session_.erase(map->GetId());
        session_id_to_session_.erase(id);
        sessions_.pop_back();
        throw;
    }
    next_session_id_ = std::max(next_session_id_, static_cast<int>(*id) + 1);
}

const Game::GameSessions& Game::GetSessions() const {
    return sessions_;
}

Game::GameSessions& Game::GetSessions() {
    return sessions_;
}

GameSession* Game::FindSession(const Map::Id& id) const noexcept {
    if (map_id_to_session_.count(id) > 0) {
        return map_id_to_session_.at(id);
    }
    return nullptr;
}

GameSession* Game::FindSession(const GameSession::Id& id) const noexcept {
    if (session_id_to_session_.count(id) > 0) {
        return session_id_to_session_.at(id);
    }
    return nullptr;
}

int Game::GetNextSessionId() const noexcept {
    return next_session_id_;
}

void Game::SetLootGeneratorConfig(LootGeneratorConfig config) {
    loot_generator_config_ = std::move(config);
}

const LootGeneratorConfig& Game::GetLootGeneratorConfig() const {
    return loot_generator_config_;
}

void Game::SetDogRetirementTime(int time) {
    dog_retirement_time_ = time;
}

int Game::GetDogRetirementTime() const {
    return dog_retirement_time_;
}

void Game::UpdateState(int time_delta) {
    if (time_delta < 0)
        throw InvalidTimeDelta("Time delta must be nonnegative");
    // Validate every session before mutating any, so a rejected tick is atomic.
    for (const auto& session : sessions_) {
        for (const auto& dog : session.GetDogs())
            dog->ValidateTimeDelta(time_delta);
    }
    for (auto& session : sessions_) {
        session.UpdateState(time_delta);
    }
}

} // namespace model
