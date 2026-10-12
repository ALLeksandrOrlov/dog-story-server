#pragma once

#include <deque>
#include <unordered_map>

#include "domain/game_session.hpp"
#include "domain/map.hpp"

namespace model
{
/** @brief Корневой агрегат: владеет картами, сессиями и индексами поиска.
 * @details Адреса карт и сессий стабильны при добавлении. Копирование запрещено;
 * перемещение допустимо до привязки внешних обработчиков к конкретному Game.
 */
class Game
{
public:
    /** @brief Каталог карт со стабильными адресами элементов. */
    using Maps = std::deque<Map>;
    /** @brief Сессии со стабильными адресами элементов. */
    using GameSessions = std::deque<GameSession>;
    /** @brief Создаёт пустую игру с базовыми настройками. */
    Game() = default;
    /** @brief Запрещает копирование взаимосвязанного графа объектов. */
    Game(const Game&) = delete;
    /** @brief Запрещает копирование владельца игровых сессий. */
    Game& operator=(const Game&) = delete;
    /** @brief Переносит агрегат с сохранением адресов его элементов. */
    Game(Game&&) = default;
    /** @brief Переносит агрегат; внешние зависимости получателя должны быть перепривязаны. */
    Game& operator=(Game&&) = default;
    /** @brief Добавляет конфигурацию с уникальным ID; существующие адреса не меняются. */
    void AddMap(const Map& map);
    /** @brief Возвращает каталог карт только для чтения. */
    const Maps& GetMaps() const noexcept;
    /** @brief Ищет карту по ID; nullptr означает отсутствие. */
    const Map* FindMap(const Map::Id& id) const noexcept;
    /** @brief Принимает владение сессией: одна сессия на карту, карта принадлежит этой игре. */
    void AddSession(GameSession session);
    /** @brief Возвращает сессии без возможности обойти поддержание внутренних индексов. */
    const GameSessions& GetSessions() const;
    /** @brief Находит изменяемую сессию карты; nullptr означает отсутствие активного мира. */
    GameSession* FindSession(const Map::Id& id) const noexcept;
    /** @brief Находит изменяемую сессию по её идентификатору. */
    GameSession* FindSession(const GameSession::Id& id) const noexcept;
    /** @brief Возвращает следующий монотонный ID сессии, не резервируя его. */
    int GetNextSessionId() const noexcept;
    /** @brief Задаёт проверенные настройки для новых сессий; существующие не изменяются. */
    void SetLootGeneratorConfig(const LootGeneratorConfig config);
    /** @brief Возвращает настройки генерации для создания сессии. */
    const LootGeneratorConfig& GetLootGeneratorConfig() const;
    /** @brief Задаёт положительный срок бездействия новых сессий в миллисекундах. */
    void SetDogRetirementTime(const int time);
    /** @brief Возвращает срок бездействия в миллисекундах. */
    int GetDogRetirementTime() const;
    /** @brief Проверяет шаг во всех сессиях, затем последовательно обновляет миры.
     * @throws InvalidTimeDelta Ни одна сессия не меняется при недопустимом шаге времени.
     * @details Ошибки внешнего обработчика завершения игры не имеют общего отката.
     */
    void UpdateState(const int time_delta);

private:
    /** @brief Хеширование типизированного идентификатора карты. */
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    /** @brief Хеширование типизированного идентификатора сессии. */
    using SessionIdHasher = util::TaggedHasher<GameSession::Id>;
    /** @brief Связь идентификатора карты с индексом в каталоге. */
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;
    /** @brief Невладеющий индекс единственной сессии каждой карты. */
    using MapIdToSession = std::unordered_map<Map::Id, GameSession*, MapIdHasher>;
    /** @brief Невладеющий индекс сессий по собственному идентификатору. */
    using SessionIdToSession = std::unordered_map<GameSession::Id, GameSession*, SessionIdHasher>;
    /** @brief Владеющее хранилище неизменяемых после добавления карт. */
    Maps m_maps{};
    /** @brief Индекс карты по её уникальному идентификатору. */
    MapIdToIndex m_map_id_to_index{};
    /** @brief Владеющее хранилище игровых сессий. */
    GameSessions m_sessions{};
    /** @brief Индекс активного мира каждой карты. */
    MapIdToSession m_map_id_to_session{};
    /** @brief Индекс сессий по их уникальным идентификаторам. */
    SessionIdToSession m_session_id_to_session{};
    /** @brief Следующий ID сессии; существующие номера не переиспользуются. */
    int m_next_session_id = 0;
    /** @brief Настройки генератора для новых сессий. */
    LootGeneratorConfig m_loot_generator_config{};
    /** @brief Срок непрерывного бездействия до выхода, в миллисекундах. */
    int m_dog_retirement_time = 60000;
};
} // namespace model
