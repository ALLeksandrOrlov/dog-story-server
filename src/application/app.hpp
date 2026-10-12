#pragma once

#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "domain/game.hpp"
#include "domain/retired_player.hpp"

#include "application/players.hpp"

namespace app
{
using model::Game;
using model::Map;

/** @brief Источник шагов симуляции: тестовый HTTP-вызов или внутренний таймер. */
enum class ApplicationMode
{
    Manual,
    Auto
};

/** @brief Причина отклонения сценария, независимая от HTTP-кодов и JSON. */
enum class ApplicationErrorReason
{
    InvalidName,
    InvalidMap,
    InvalidToken,
    InvalidDirection,
    InvalidState,
    InvalidTimeDelta,
    InvalidMaxItems
};

/** @brief Ошибка сценария; контроллер переводит причину в публичный ответ API. */
class ApplicationError : public std::exception
{
public:
    /** @brief Сохраняет причину и диагностическое сообщение без привязки к транспорту. */
    ApplicationError(const ApplicationErrorReason reason, const std::string& message):
            m_reason(reason),
            m_message(message)
    {
    }
    /** @brief Возвращает диагностическое сообщение по контракту std::exception. */
    const char* what() const noexcept override
    {
        return m_message.c_str();
    }
    /** @brief Возвращает стабильную причину для отображения в транспортную ошибку. */
    ApplicationErrorReason GetReason() const noexcept
    {
        return m_reason;
    }

private:
    /** @brief Причина отклонения сценария для транспортного отображения. */
    ApplicationErrorReason m_reason;
    /** @brief Текст исключения, доступный через стандартный what(). */
    std::string m_message{};
};

/** @brief Краткое описание карты в каталоге. */
struct MapSummary
{
    /** @brief Публичный идентификатор описываемой сущности. */
    std::string id{};
    /** @brief Отображаемое имя сущности. */
    std::string name{};
};
/** @brief Результат чтения каталога доступных карт. */
struct GetMapsResult
{
    /** @brief Краткие описания доступных карт. */
    std::vector<MapSummary> maps{};
};
/** @brief Проверенный идентификатор карты для представления её конфигурации. */
struct GetMapDescResult
{
    /** @brief Публичный идентификатор описываемой сущности. */
    model::Map::Id id{std::string{}};
};
/** @brief Регистрационные данные успешно присоединившегося игрока. */
struct JoinGameResult
{
    /** @brief Секрет авторизации нового игрока. */
    Token token{std::string{}};
    /** @brief Идентификатор успешно зарегистрированного игрока. */
    Player::Id player_id{0};
};
/** @brief Публичная идентичность игрока без токена авторизации. */
struct PlayerSummary
{
    /** @brief Публичный идентификатор описываемой сущности. */
    Player::Id id{0};
    /** @brief Отображаемое имя сущности. */
    std::string name{};
};
/** @brief Игроки одной сессии, видимые авторизованному участнику. */
struct GetPlayersListResult
{
    /** @brief Снимки игроков текущей сессии. */
    std::vector<PlayerSummary> players{};
};
/** @brief Снимок состояния одного игрока для сериализации ответа. */
struct PlayerState
{
    /** @brief Публичный идентификатор описываемой сущности. */
    Player::Id id{0};
    /** @brief Положение в координатах карты. */
    model::Position position{};
    /** @brief Вектор скорости в единицах карты в секунду. */
    geom::Vec2D speed{};
    /** @brief Ориентация U/D/L/R. */
    std::string direction{};
    /** @brief Предметы, находящиеся у игрока. */
    std::vector<model::Loot> bag{};
    /** @brief Неотрицательные очки за доставленные предметы. */
    int score = 0;
};
/** @brief Независимый снимок игроков и предметов одной игровой сессии. */
struct GetGameStateResult
{
    /** @brief Снимки игроков текущей сессии. */
    std::vector<PlayerState> players{};
    /** @brief Предметы, ещё не подобранные на карте. */
    model::GameSession::Loots lost_objects{};
};
/** @brief Упорядоченная страница сохранённых результатов игры. */
struct GetGameRecordsResult
{
    /** @brief Результаты завершённых игр в порядке рейтинга. */
    std::vector<domain::RetiredPlayer> records{};
};

/** @brief Координирует сценарии игры, авторизацию и порт сохранения результатов.
 * @details Не знает о HTTP, JSON и реализации БД. Все вызовы выполняются
 * последовательно одним исполнителем. Game, Players и Repository должны жить
 * дольше Application и привязанных обработчиков завершения игры.
 */
class Application
{
public:
    /** @brief Внедряемый источник токенов авторизации без зависимости от транспорта. */
    using TokenGenerator = std::function<Token()>;
    /** @brief Привязывает обработчики существующих сессий и внедрённые зависимости.
     * @param game Доменный мир; сохраняет стабильность карт и сессий.
     * @param players Реестр игроков, связанный с этим миром.
     * @param mode Разрешённый источник тактов симуляции.
     * @param is_random_spawn_point Выбирает случайную точку вместо начала первой дороги.
     * @param players_repo Порт сохранения завершённых игр.
     * @param on_update Вызывается после успешного такта; используется для сохранения снимков.
     * @param token_generator Генератор секретов; может быть подменён в тесте.
     */
    Application(Game& game, Players& players, const ApplicationMode mode, const bool is_random_spawn_point,
            const std::function<void(int)>& on_update, domain::RetiredPlayerRepository& players_repo,
            const TokenGenerator& token_generator = RandomToken::Generate);
    /** @brief Запрещает копирование координатора внешних зависимостей. */
    Application(const Application&) = delete;
    /** @brief Запрещает замену зависимостей копирующим присваиванием. */
    Application& operator=(const Application&) = delete;
    /** @brief Возвращает каталог карт без изменяемых доменных объектов. */
    GetMapsResult GetMaps() const;
    /** @brief Проверяет наличие карты и возвращает её идентификатор для представления. */
    GetMapDescResult GetMapDesc(const Map::Id& map_id) const;
    /** @brief Создаёт игрока и собаку, выдавая уникальный секрет авторизации.
     * @param map_id Идентификатор существующей карты.
     * @param user_name Непустое имя без NUL; обычные Unicode-символы и длинные имена допустимы.
     * @throws ApplicationError Имя пусто, содержит NUL либо карта отсутствует.
     * @details Ошибка регистрации не оставляет собаку без игрока; использованный ID не переиспользуется.
     */
    JoinGameResult JoinGame(const Map::Id& map_id, const std::string_view user_name);
    /** @brief Возвращает только игроков сессии владельца токена. */
    GetPlayersListResult GetPlayersList(const Token& token) const;
    /** @brief Возвращает снимок только сессии владельца токена. */
    GetGameStateResult GetGameState(const Token& token) const;
    /** @brief Проверяет авторизацию и применяет направление U/D/L/R либо остановку. */
    void DoAction(const Token& token, const std::string& direction);
    /** @brief Продвигает ручную симуляцию; запрещён при автоматическом таймере. */
    void ManualTick(const int time_delta);
    /** @brief Продвигает автоматическую симуляцию; запрещён в ручном режиме. */
    void AutoTick(const int time_delta);
    /** @brief Читает страницу результатов: offset >= 0, размер страницы от 0 до 100. */
    GetGameRecordsResult GetGameRecords(const std::optional<int> start, const std::optional<int> max_items);
    /** @brief Проверяет синтаксис токена: ровно 32 шестнадцатеричных символа. */
    static bool IsValidToken(const Token& token);

private:
    /** @brief Невладеющая ссылка на доменный мир; живёт дольше обработчиков сессий. */
    Game& m_game;
    /** @brief Реестр зарегистрированных игроков. */
    Players& m_players;
    /** @brief Разрешённый источник тактов симуляции. */
    ApplicationMode m_mode = ApplicationMode::Manual;
    /** @brief Признак случайного выбора точки входа на дорогу. */
    bool m_is_random_spawn_point = false;
    /** @brief Обработчик успешного такта для периодического сохранения снимка. */
    std::function<void(int)> m_on_update{};
    /** @brief Невладеющая ссылка на порт хранения завершённых игр. */
    domain::RetiredPlayerRepository& m_players_repo;
    /** @brief Источник секретов, подменяемый для проверки ошибок регистрации. */
    TokenGenerator m_token_generator{};

    /** @brief Проверяет источник такта и преобразует доменную ошибку времени в ошибку сценария. */
    void Tick(const int time_delta, const ApplicationMode expected_mode);
    /** @brief Возвращает авторизованного игрока или бросает ApplicationError. */
    Players::PlayerPtr RequirePlayer(const Token& token) const;
    /** @brief Регистрирует сохранение результата до удаления игрока и его токена. */
    void BindSession(GameSession& session);
    /** @brief Выбирает свободный токен; число попыток ограничено при неисправном генераторе. */
    Token GenerateUniqueToken() const;
};
} // namespace app
