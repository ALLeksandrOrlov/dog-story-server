#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "domain/game.hpp"

namespace model
{
using namespace std::literals;
void Game::AddMap(const Map& map)
{
    const size_t index = m_maps.size();
    if (auto [it, inserted] = m_map_id_to_index.emplace(map.GetId(), index); !inserted)
    {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    }
    else
    {
        try
        {
            m_maps.emplace_back(map);
        }
        catch(std::exception&)
        {
            m_map_id_to_index.erase(it);
            throw;
        }
    }
}

const Game::Maps& Game::GetMaps() const noexcept
{
    return m_maps;
}

const Map* Game::FindMap(const Map::Id& id) const noexcept
{
    if (auto it = m_map_id_to_index.find(id); it != m_map_id_to_index.end())
    {
        return &m_maps.at(it->second);
    }
    return nullptr;
}

void Game::AddSession(GameSession session)
{
    const auto id = session.GetId();
    const auto* map = session.GetMap();
    if (!map || FindMap(map->GetId()) != map)
    {
        throw std::invalid_argument("Session map must belong to this game");
    }
    if (m_session_id_to_session.contains(id) || m_map_id_to_session.contains(map->GetId()))
    {
        throw std::invalid_argument("Duplicate session or session map");
    }
    if (*id >= static_cast<unsigned>(std::numeric_limits<int>::max()))
    {
        throw std::invalid_argument("Session id is outside the supported range");
    }
    m_sessions.push_back(std::move(session));
    try
    {
        m_map_id_to_session.emplace(map->GetId(), &m_sessions.back());
        m_session_id_to_session.emplace(id, &m_sessions.back());
    }
    catch(...)
    {
        m_map_id_to_session.erase(map->GetId());
        m_session_id_to_session.erase(id);
        m_sessions.pop_back();
        throw;
    }
    m_next_session_id = std::max(m_next_session_id, static_cast<int>(*id) + 1);
}

const Game::GameSessions& Game::GetSessions() const
{
    return m_sessions;
}

GameSession* Game::FindSession(const Map::Id& id) const noexcept
{
    if (m_map_id_to_session.count(id) > 0)
    {
        return m_map_id_to_session.at(id);
    }
    return nullptr;
}

GameSession* Game::FindSession(const GameSession::Id& id) const noexcept
{
    if (m_session_id_to_session.count(id) > 0)
    {
        return m_session_id_to_session.at(id);
    }
    return nullptr;
}

int Game::GetNextSessionId() const noexcept
{
    return m_next_session_id;
}

void Game::SetLootGeneratorConfig(const LootGeneratorConfig config)
{
    constexpr double milliseconds_per_second = 1000.0;
    if (!std::isfinite(config.period) || config.period < 1.0 / milliseconds_per_second ||
            config.period > std::numeric_limits<int>::max() / milliseconds_per_second ||
            !std::isfinite(config.probability) || config.probability < 0.0 || config.probability > 1.0)
    {
        throw std::invalid_argument("Invalid loot generator configuration");
    }
    m_loot_generator_config = config;
}

const LootGeneratorConfig& Game::GetLootGeneratorConfig() const
{
    return m_loot_generator_config;
}

void Game::SetDogRetirementTime(const int time)
{
    if (time <= 0)
    {
        throw std::invalid_argument("Dog retirement time must be positive");
    }
    m_dog_retirement_time = time;
}

int Game::GetDogRetirementTime() const
{
    return m_dog_retirement_time;
}

void Game::UpdateState(const int time_delta)
{
    if (time_delta < 0)
    {
        throw InvalidTimeDelta("Time delta must be nonnegative");
    }
    // Validate every session before mutating any, so a rejected tick is atomic.
    for(const auto& session : m_sessions)
    {
        for(const auto& dog : session.GetDogs())
        {
            dog->ValidateTimeDelta(time_delta);
        }
    }
    for(auto& session : m_sessions)
    {
        session.UpdateState(time_delta);
    }
}

} // namespace model
