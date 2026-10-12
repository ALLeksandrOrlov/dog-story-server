#pragma once
#include <boost/json.hpp>

#include "application/app.hpp"

namespace view
{
/**
 * @brief Преобразует значения прикладного слоя в публичный формат JSON.
 * @details Каталог описаний карт копируется из конфигурации один раз и далее неизменяем.
 * Он содержит клиентские метаданные (модели и цвета), которые не нужны доменным алгоритмам.
 * Представление не обращается к изменяемой игре или базе данных.
 */
class JsonView
{
public:
    /** @brief Копирует описания карт из проверенной конфигурации; исходный JSON не заимствуется. */
    explicit JsonView(const boost::json::value& config);
    /** @brief Возвращает массив идентификаторов и названий карт. */
    boost::json::array Render(const app::GetMapsResult& result) const;
    /** @brief Возвращает описание карты, включая визуальные свойства предметов.
     * @throws std::out_of_range Идентификатор отсутствует в каталоге описаний.
     */
    boost::json::object Render(const app::GetMapDescResult& result) const;
    /** @brief Возвращает authToken и playerId новой сессии игрока. */
    boost::json::object Render(const app::JoinGameResult& result) const;
    /** @brief Возвращает объект с именами игроков, индексированный строковым ID. */
    boost::json::object Render(const app::GetPlayersListResult& result) const;
    /** @brief Сериализует положение, скорость, сумку, очки и свободные предметы. */
    boost::json::object Render(const app::GetGameStateResult& result) const;
    /** @brief Возвращает рекорды с переводом play_time_ms в секунды. */
    boost::json::array Render(const app::GetGameRecordsResult& result) const;

private:
    /** @brief Неизменяемые клиентские описания карт, индексированные ID. */
    std::unordered_map<std::string, boost::json::object> m_map_descriptions;
};
} // namespace view
