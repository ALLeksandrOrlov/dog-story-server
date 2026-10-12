#pragma once

#include <memory>
#include <string_view>

#include "domain/retired_player.hpp"

namespace postgres
{
/**
 * @brief Владеет подключением PostgreSQL и репозиторием завершённых игр.
 * @details SQL и типы драйвера скрыты в реализации. Все обращения выполняются
 * последовательно на игровом strand. Репозиторий действителен до уничтожения Database.
 */
class Database
{
public:
    /** @brief Открывает подключение и подготавливает схему; ошибки драйвера передаются вызывающему коду. */
    explicit Database(const std::string_view connection_url);
    /** @brief Закрывает подключение после освобождения репозитория. */
    ~Database();
    /** @brief Подключение имеет единственного владельца. */
    Database(const Database&) = delete;
    /** @brief Перемещение запрещено, чтобы заимствованные ссылки сохраняли срок жизни владельца. */
    Database(Database&&) = delete;
    /** @brief Копирование подключения запрещено. */
    Database& operator=(const Database&) = delete;
    /** @brief Перемещение подключения запрещено. */
    Database& operator=(Database&&) = delete;

    /** @brief Возвращает заимствованный порт хранения результатов; не передаёт владение. */
    domain::RetiredPlayerRepository& GetRetiredPlayer();

private:
    /** @brief Скрывает драйвер, подключение и SQL-адаптер от публичного интерфейса. */
    class Impl;
    /** @brief Единственный владелец скрытого подключения и связанного с ним репозитория. */
    std::unique_ptr<Impl> m_impl;
};
} // namespace postgres
