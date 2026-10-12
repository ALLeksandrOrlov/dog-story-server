#pragma once

#include <boost/serialization/vector.hpp>

#include "application/players.hpp"
#include "domain/game.hpp"

namespace serialization
{

/** @brief Сохраняет связь токена игрока с картой и собакой через идентификаторы. */
class PlayerRepr
{
public:
    /** @brief Инициализирует поля перед чтением архива. */
    PlayerRepr() = default;

    /** @brief Копирует идентификаторы игрока без адресов объектов игрового мира. */
    explicit PlayerRepr(const app::Player& player):
            m_id(player.GetId()),
            m_token(player.GetToken()),
            m_map_id(player.GetSession()->GetMap()->GetId()),
            m_dog_id(player.GetDog()->GetId())
    {
    }

    /**
     * @brief Восстанавливает игрока после загрузки игровых сессий.
     * @throws std::runtime_error Если сессия или собака отсутствует в переданной игре.
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

    /** @brief Сериализует поля в порядке архивного формата; имя требуется Boost.Serialization. */
    template <typename Archive> void serialize(Archive& ar, [[maybe_unused]] const unsigned version)
    {
        /** @brief Идентификатор игрока, уникальный в пределах игры. */
        ar&* m_id;
        /** @brief Секрет авторизации, сохраняемый для продолжения сессии после перезапуска. */
        ar&* m_token;
        /** @brief Идентификатор карты, по которому восстанавливается ссылка на сессию. */
        ar&* m_map_id;
        /** @brief Идентификатор собаки внутри восстановленной сессии. */
        ar&* m_dog_id;
    }

private:
    /** @brief Идентификатор игрока, уникальный в пределах игры. */
    app::Player::Id m_id{0};
    /** @brief Секрет авторизации, сохраняемый для продолжения сессии после перезапуска. */
    app::Token m_token{""};
    /** @brief Идентификатор карты, по которому восстанавливается ссылка на сессию. */
    model::Map::Id m_map_id{""};
    /** @brief Идентификатор собаки внутри восстановленной сессии. */
    model::Dog::Id m_dog_id{0};
};

/** @brief Снимок реестра игроков, восстанавливаемый после игрового мира. */
class PlayersRepr
{
public:
    /** @brief Создаёт пустой реестр для чтения из архива. */
    PlayersRepr() = default;

    /** @brief Копирует связи всех зарегистрированных игроков. */
    explicit PlayersRepr(const app::Players& players)
    {
        const auto players_list = players.GetPlayersList();
        for(const auto& player : players_list)
        {
            m_players.emplace_back(*player);
        }
    }

    /** @brief Собирает отдельный реестр, проверяя ссылки и уникальность идентификаторов. */
    app::Players Restore(const model::Game& game) const
    {
        app::Players players;
        for(const auto& player : m_players)
        {
            players.AddPlayer(player.Restore(game));
        }
        return players;
    }

    /** @brief Сериализует поля в порядке архивного формата; имя требуется Boost.Serialization. */
    template <typename Archive> void serialize(Archive& ar, [[maybe_unused]] const unsigned version)
    {
        /** @brief Архивные связи всех зарегистрированных игроков. */
        ar & m_players;
    }

private:
    /** @brief Архивные связи всех зарегистрированных игроков. */
    std::vector<PlayerRepr> m_players;
};

} // namespace serialization
