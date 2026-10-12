#pragma once

#include <string>
#include <vector>

#include "common/tagged_uuid.hpp"

namespace domain
{
/** @brief Завершённая игра: независимый от активной сессии результат для хранения. */
struct RetiredPlayer
{
    /** @brief Публичный идентификатор описываемой сущности. */
    std::string id{};
    /** @brief Отображаемое имя сущности. */
    std::string name{};
    /** @brief Неотрицательные очки за доставленные предметы. */
    int score = 0;
    /** @brief Длительность завершённой игры в миллисекундах. */
    int play_time_ms = 0;
};
namespace detail
{
/** @brief Типовая метка уникального идентификатора завершённой игры. */
struct RetiredPlayerTag
{
};
} // namespace detail
/** @brief Уникальная идентичность завершённой игры, независимая от ID активного игрока. */
using RetiredPlayerId = util::TaggedUUID<detail::RetiredPlayerTag>;

/** @brief Порт хранения завершённых игр, независимый от конкретной базы данных.
 * @details Ошибки инфраструктуры передаются вызывающему слою; домен не знает SQL.
 */
class RetiredPlayerRepository
{
public:
    /** @brief Разрешает безопасное владение реализацией через базовый интерфейс. */
    virtual ~RetiredPlayerRepository() = default;
    /** @brief Сохраняет завершённую игру до удаления активного игрока.
     * @details Успешный возврат означает завершение операции хранения данной реализацией.
     */
    virtual void SaveRetiredPlayer(const RetiredPlayer& player) = 0;
    /** @brief Читает страницу результатов: счёт убывает, затем время и имя возрастают.
     * @pre offset и limit неотрицательны; вызывающий слой проверяет размер страницы.
     */
    virtual std::vector<RetiredPlayer> GetRetiredPlayers(const int offset, const int limit) = 0;
};
} // namespace domain
