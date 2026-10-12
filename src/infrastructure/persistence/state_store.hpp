#pragma once

#include <chrono>
#include <filesystem>
#include <optional>

#include "application/players.hpp"

namespace persistence
{
/**
 * @brief Сохраняет активную игру в файловый снимок отдельно от таблицы результатов.
 * @details Save и OnTick вызываются последовательно после игрового обновления;
 * Restore — только до запуска приложения. Пустой путь отключает хранение.
 */
class StateStore
{
public:
    /** @brief Задаёт путь снимка и необязательный положительный период игрового времени. */
    StateStore(std::filesystem::path path, const std::optional<int> period_ms);
    /**
     * @brief Восстанавливает снимок в игру без активных сессий и игроков.
     * @details При ошибке исходные объекты сохраняются. Отсутствующий файл допустим.
     * После восстановления скорость движения и таймеры игроков равны нулю.
     */
    void Restore(model::Game& game, app::Players& players) const;
    /**
     * @brief Записывает временный файл и заменяет снимок только после успешной сериализации.
     * @details Ошибки записи передаются вызывающему коду; это атомарная замена файла,
     * а не общая транзакция с PostgreSQL и не гарантия сохранности при потере питания.
     */
    void Save(const model::Game& game, const app::Players& players) const;
    /** @brief Сохраняет состояние после превышения периода; не сбрасывает отсчёт при ошибке записи. */
    void OnTick(const int delta_ms, const model::Game& game, const app::Players& players);

private:
    /** @brief Путь итогового снимка; пустой путь отключает запись и восстановление. */
    std::filesystem::path m_path;
    /** @brief Необязательный положительный период по игровому времени, в миллисекундах. */
    std::optional<int> m_period_ms;
    /** @brief Игровое время, накопленное после создания хранилища. */
    std::chrono::milliseconds m_elapsed{0};
    /** @brief Игровое время последней успешной замены файла снимка. */
    std::chrono::milliseconds m_last_save{0};
};
} // namespace persistence
