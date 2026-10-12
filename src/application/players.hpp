#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/tagged.hpp"
#include "domain/game_session.hpp"

namespace app
{
namespace detail
{
/** @brief Разделяет тип токена доступа и обычные строки на этапе компиляции. */
struct TokenTag
{
};
} // namespace detail

/** @brief Секрет авторизации; отдельный тип предотвращает смешение с обычными строками. */
using Token = util::Tagged<std::string, detail::TokenTag>;

using model::Dog;
using model::GameSession;

/** @brief Создаёт токены доступа непосредственно из системного источника случайности.
 * @details На поддерживаемых Linux/macOS std::random_device использует источник ОС.
 * Промежуточный PRNG с коротким seed не применяется.
 */
class RandomToken
{
public:
    /** @brief Запрещает создание экземпляров утилиты без состояния. */
    RandomToken() = delete;
    /** @brief Возвращает 32 шестнадцатеричных символа; ошибку источника передаёт вызывающему коду. */
    static Token Generate();
};

/** @brief Связывает авторизацию игрока с собакой и сессией.
 * @details Удерживает собаку через shared_ptr, но сессией не владеет. Сессия
 * обязана пережить игрока; реестр Players проверяет принадлежность собаки сессии.
 */
class Player
{
public:
    /** @brief Строго типизированный идентификатор сущности. */
    using Id = util::Tagged<std::uint32_t, Player>;
    /** @brief Создаёт запись для последующей проверяемой регистрации в Players. */
    Player(const Id id, const Token& token, GameSession* session, const GameSession::DogPtr& dog);
    /** @brief Возвращает уникальный в игре идентификатор игрока. */
    const Id& GetId() const noexcept;
    /** @brief Преобразует идентификатор в десятичный ключ ответа. */
    std::string GetIdAsString() const;
    /** @brief Возвращает секрет авторизации, действующий до удаления регистрации. */
    const Token& GetToken() const;
    /** @brief Возвращает имя связанной собаки. */
    std::string GetName() const;
    /** @brief Возвращает невладеющий указатель на сессию игрока. */
    GameSession* GetSession() const;
    /** @brief Возвращает разделяемую ссылку на собаку игрока. */
    GameSession::DogPtr GetDog() const;

private:
    /** @brief Идентификатор сущности, не изменяемый после регистрации. */
    Id m_id{0};
    /** @brief Токен доступа связанного игрока. */
    Token m_token{std::string{}};
    /** @brief Невладеющий указатель на сессию; Game должен пережить игрока. */
    GameSession* m_session = nullptr;
    /** @brief Собака, управляемая авторизованным игроком. */
    GameSession::DogPtr m_dog{};
};

/** @brief Реестр авторизации и привязок игроков к доменному миру.
 * @details Доступ сериализуется с игровыми действиями. Копирование запрещено,
 * поскольку независимые реестры не должны разделять изменяемых игроков.
 */
class Players
{
public:
    /** @brief Разделяемое владение зарегистрированным игроком. */
    using PlayerPtr = std::shared_ptr<Player>;
    /** @brief Создаёт пустой реестр. */
    Players() = default;
    /** @brief Запрещает неявное разделение игроков между копиями реестра. */
    Players(const Players&) = delete;
    /** @brief Запрещает копирующее присваивание реестра. */
    Players& operator=(const Players&) = delete;
    /** @brief Переносит реестр с сохранением привязок к сессиям. */
    Players(Players&&) = default;
    /** @brief Переносит реестр; получатель принимает существующие привязки к миру. */
    Players& operator=(Players&&) = default;
    /** @brief Атомарно добавляет игрока во все индексы.
     * @throws std::invalid_argument Повторяется ID/токен/собака либо нарушена привязка к сессии.
     */
    void AddPlayer(const Player& player);
    /** @brief Возвращает следующий монотонный идентификатор, не резервируя его. */
    int GetNextPlayerId() const noexcept;
    /** @brief Ищет авторизованного игрока; nullptr означает неизвестный или отозванный токен. */
    PlayerPtr FindByToken(const Token& token) const;
    /** @brief Создаёт список игроков только указанной сессии. */
    std::vector<PlayerPtr> GetSessionPlayersList(const GameSession& session) const;
    /** @brief Предоставляет реестр для создания снимка без возможности его структурного изменения. */
    const std::deque<PlayerPtr>& GetPlayersList() const;
    /** @brief Удаляет привязку собаки и отзывает токен; отсутствие записи допустимо. */
    void RemovePlayer(const GameSession::Id session_id, const Dog::Id dog_id);

private:
    /** @brief Хеширование типизированного токена для индекса авторизации. */
    using TokenIdHasher = util::TaggedHasher<Token>;
    /** @brief Индекс авторизации: токен — зарегистрированный игрок. */
    using TokenToPlayer = std::unordered_map<Token, PlayerPtr, TokenIdHasher>;
    /** @brief Реестр зарегистрированных игроков. */
    std::deque<PlayerPtr> m_players{};
    /** @brief Индекс действующих токенов авторизации. */
    TokenToPlayer m_token_to_player{};
    /** @brief Следующий ID игрока; удалённые ID не переиспользуются. */
    int m_next_player_id = 0;
};
} // namespace app
