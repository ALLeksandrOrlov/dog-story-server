#pragma once

#include "application/app.hpp"
#include "view/json_view.hpp"

#include "controller/http_types.hpp"

namespace http_handler
{
/**
 * @brief Проверяет HTTP-запросы и преобразует результаты прикладных операций в ответы API.
 * @details Не хранит игровое состояние. Application и JsonView заимствуются и должны жить дольше
 * контроллера. Все вызовы Handle выполняются последовательно в игровом strand.
 */
class ApiController
{
public:
    /** @brief Связывает прикладные операции с неизменяемым JSON-представлением. */
    ApiController(app::Application& application, const view::JsonView& view):
            m_application(application),
            m_view(view)
    {
    }

    /**
     * @brief Выполняет маршрут и формирует HTTP-ответ по контракту полей и статусов.
     * @param request Запрос с телом JSON и заголовками авторизации.
     * @param uri Декодированный адрес, включая параметры запроса.
     * @throws std::exception Ошибка инфраструктуры; внешний обработчик преобразует её в HTTP 500.
     */
    ApiRequestResult Handle(const StringRequest& request, const std::string_view uri) const;

private:
    /** @brief Заимствованный фасад сценариев игры. */
    app::Application& m_application;
    /** @brief Заимствованное неизменяемое JSON-представление. */
    const view::JsonView& m_view;

    /** @brief Определяет операцию по пути без выполнения прикладной логики. */
    static ApiRequestType ClassifyRequest(const std::string_view target);
    /** @brief Возвращает список доступных карт. */
    ApiRequestResult HandleMapsList(const StringRequest& request, const std::string_view uri) const;
    /** @brief Возвращает описание карты либо ответ об неизвестном идентификаторе. */
    ApiRequestResult HandleMapDescription(const StringRequest& request, const std::string_view uri) const;
    /** @brief Проверяет имя и карту, затем регистрирует игрока. */
    ApiRequestResult HandleJoinGame(const StringRequest& request, const std::string_view uri) const;
    /** @brief Возвращает игроков сессии, определяемой токеном запроса. */
    ApiRequestResult HandlePlayersList(const StringRequest& request, const std::string_view uri) const;
    /** @brief Возвращает снимок игроков и предметов авторизованной сессии. */
    ApiRequestResult HandleGameState(const StringRequest& request, const std::string_view uri) const;
    /** @brief Передаёт проверенную команду движения в Application. */
    ApiRequestResult HandleAction(const StringRequest& request, const std::string_view uri) const;
    /** @brief Проверяет длительность и выполняет ручной игровой такт. */
    ApiRequestResult HandleTick(const StringRequest& request, const std::string_view uri) const;
    /** @brief Разбирает параметры страницы и возвращает таблицу рекордов. */
    ApiRequestResult HandleGameRecords(const StringRequest& request, const std::string_view uri) const;
    /** @brief Формирует ответ для неизвестного маршрута или недопустимых параметров. */
    static ApiRequestResult HandleInvalidRequest(const StringRequest& request, const std::string_view uri);
};
} // namespace http_handler
