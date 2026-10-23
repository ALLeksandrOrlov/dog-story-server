#pragma once

#include <boost/serialization/vector.hpp>

#include "application/players.hpp"
#include "domain/game.hpp"

namespace serialization
{

/** @brief Stores the association between a player token, map, and dog using identifiers. */
class PlayerRepr
{
public:
    /** @brief Initializes fields before reading the archive. */
    PlayerRepr() = default;

    /** @brief Copies player identifiers without storing game world object addresses. */
    explicit PlayerRepr(const app::Player& player):
            m_id(player.GetId()),
            m_token(player.GetToken()),
            m_map_id(player.GetSession()->GetMap()->GetId()),
            m_dog_id(player.GetDog()->GetId())
    {
    }

    /**
     * @brief Restores a player after loading game sessions.
     * @throws std::runtime_error If the session or dog is missing from the supplied game.
     */
    app::Player Restore(const model::Game& game) const
    {
        model::GameSession* session = game.FindSession(m_map_id);
        if (!session)
        {
            throw std::runtime_error("Snapshot player references an unknown session");
        }
        auto dog = session->FindDog(m_dog_id);
        if (!dog)
        {
            throw std::runtime_error("Snapshot player references an unknown dog");
        }
        return {m_id, m_token, session, std::move(dog)};
    }

    /** @brief Serializes fields in archive order; the name is required by Boost.Serialization. */
    template <typename Archive> void serialize(Archive& ar, [[maybe_unused]] const unsigned version)
    {
        /** @brief Player identifier, unique within the game. */
        ar&* m_id;
        /** @brief Authorization secret saved to resume the session after a restart. */
        ar&* m_token;
        /** @brief Map identifier used to restore the session reference. */
        ar&* m_map_id;
        /** @brief Dog identifier within the restored session. */
        ar&* m_dog_id;
    }

private:
    /** @brief Player identifier, unique within the game. */
    app::Player::Id m_id{0};
    /** @brief Authorization secret saved to resume the session after a restart. */
    app::Token m_token{""};
    /** @brief Map identifier used to restore the session reference. */
    model::Map::Id m_map_id{""};
    /** @brief Dog identifier within the restored session. */
    model::Dog::Id m_dog_id{0};
};

/** @brief Player registry snapshot restored after the game world. */
class PlayersRepr
{
public:
    /** @brief Creates an empty registry for archive loading. */
    PlayersRepr() = default;

    /** @brief Copies the associations of all registered players. */
    explicit PlayersRepr(const app::Players& players)
    {
        const auto players_list = players.GetPlayersList();
        for(const auto& player : players_list)
        {
            m_players.emplace_back(*player);
        }
    }

    /** @brief Builds a separate registry, validating references and identifier uniqueness. */
    app::Players Restore(const model::Game& game) const
    {
        app::Players players;
        for(const auto& player : m_players)
        {
            players.AddPlayer(player.Restore(game));
        }
        return players;
    }

    /** @brief Serializes fields in archive order; the name is required by Boost.Serialization. */
    template <typename Archive> void serialize(Archive& ar, [[maybe_unused]] const unsigned version)
    {
        /** @brief Archived associations of all registered players. */
        ar & m_players;
    }

private:
    /** @brief Archived associations of all registered players. */
    std::vector<PlayerRepr> m_players;
};

} // namespace serialization
