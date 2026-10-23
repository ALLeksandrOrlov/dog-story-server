#include <algorithm>
#include <limits>
#include <random>

#include "application/players.hpp"

namespace app
{
using namespace std::literals;
namespace
{
/** @brief Generated token length in hexadecimal characters. */
constexpr size_t token_size = 32;
/** @brief Alphabet used for hexadecimal token encoding. */
constexpr char hex_digits[] = "0123456789abcdef";
} // namespace

Token RandomToken::Generate()
{
    std::random_device random_device;
    std::uniform_int_distribution<unsigned> distribution(0, 15);
    std::string token(token_size, '0');
    for(auto& digit : token)
    {
        digit = hex_digits[distribution(random_device)];
    }
    return Token(std::move(token));
}

Player::Player(const Id id, const Token& token, GameSession* session, const GameSession::DogPtr& dog):
        m_id(id),
        m_token(token),
        m_session(session),
        m_dog(dog)
{
}

const Player::Id& Player::GetId() const noexcept
{
    return m_id;
}

std::string Player::GetIdAsString() const
{
    return std::to_string(*m_id);
}

const Token& Player::GetToken() const
{
    return m_token;
}

std::string Player::GetName() const
{
    return m_dog->GetName();
}

GameSession* Player::GetSession() const
{
    return m_session;
}

GameSession::DogPtr Player::GetDog() const
{
    return m_dog;
}

void Players::AddPlayer(const Player& player)
{
    const auto id = player.GetId();
    const auto token = player.GetToken();
    if (!player.GetSession() || !player.GetDog() ||
            player.GetSession()->FindDog(player.GetDog()->GetId()) != player.GetDog())
    {
        throw std::invalid_argument("Player must reference a dog in its session");
    }
    if (*id >= static_cast<unsigned>(std::numeric_limits<int>::max()))
    {
        throw std::invalid_argument("Player id is outside the supported range");
    }
    if (m_token_to_player.contains(token) ||
            std::any_of(m_players.begin(), m_players.end(),
                    [&](const auto& existing)
                    {
                        return existing->GetId() == id ||
                                (existing->GetSession() == player.GetSession() &&
                                        existing->GetDog() == player.GetDog());
                    }))
    {
        throw std::invalid_argument("Duplicate player identity");
    }
    auto next_player = std::make_shared<Player>(player);
    m_token_to_player.emplace(token, next_player);
    try
    {
        m_players.push_back(next_player);
    }
    catch(...)
    {
        m_token_to_player.erase(token);
        throw;
    }
    m_next_player_id = std::max(m_next_player_id, static_cast<int>(*id) + 1);
}

int Players::GetNextPlayerId() const noexcept
{
    return m_next_player_id;
}

Players::PlayerPtr Players::FindByToken(const Token& token) const
{
    if (m_token_to_player.count(token) > 0)
    {
        return m_token_to_player.at(token);
    }
    return nullptr;
}

std::vector<Players::PlayerPtr> Players::GetSessionPlayersList(const GameSession& session) const
{
    std::vector<PlayerPtr> list;
    for(auto& player : m_players)
    {
        const GameSession* next_session = player->GetSession();
        if (next_session == &session)
        {
            list.push_back(player);
        }
    }
    return list;
}

const std::deque<Players::PlayerPtr>& Players::GetPlayersList() const
{
    return m_players;
}

void Players::RemovePlayer(const GameSession::Id session_id, const Dog::Id dog_id)
{
    size_t idx = 0;
    for(const auto player : m_players)
    {
        auto session = player->GetSession();
        auto dog = player->GetDog();
        if ((session->GetId() == session_id) && (dog->GetId() == dog_id))
        {
            Token pl_token = m_players.at(idx)->GetToken();
            m_players.erase(m_players.begin() + idx);
            m_token_to_player.erase(pl_token);
            break;
        }
        idx++;
    }
}

} // namespace app
