#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "common/tagged.hpp"

#include "domain/geom.hpp"
#include "domain/loot.hpp"

namespace model
{
/** @brief Отклонённый шаг времени, который нельзя представить без переполнения. */
class InvalidTimeDelta : public std::invalid_argument
{
public:
    /** @brief Передаёт диагностическое сообщение стандартному исключению. */
    using std::invalid_argument::invalid_argument;
};

/** @brief Состояние игровой собаки: движение, сумка, счёт и время жизни.
 * @details При создании собака стоит; сохранённое направление задаёт ориентацию,
 * а не скорость. Это сохраняет контракт восстановления снимков.
 */
class Dog
{
public:
    /** @brief Строго типизированный идентификатор сущности. */
    using Id = util::Tagged<std::uint32_t, Dog>;
    /** @brief Создаёт стоящую собаку с указанными параметрами.
     * @throws std::invalid_argument Позиция/скорость неконечны, направление недопустимо,
     * либо скорость, вместимость или счёт отрицательны.
     */
    Dog(const Id id, const std::string& name, const Position spawn_point, const double abs_speed,
            const std::string& direction, const int bag_capacity, const int score);
    /** @brief Возвращает идентификатор, уникальный в пределах сессии. */
    const Id& GetId() const noexcept;
    /** @brief Возвращает имя, предоставленное игроком при входе. */
    const std::string& GetName() const noexcept;
    /** @brief Устанавливает конечные координаты; принадлежность дороге проверяет сессия. */
    void SetPosition(const Position position);
    /** @brief Возвращает текущую точку в координатах карты. */
    Position GetPosition() const noexcept;
    /** @brief Возвращает вектор движения в единицах карты в секунду. */
    geom::Vec2D GetSpeed() const noexcept;
    /** @brief Возвращает заданный модуль скорости, в том числе в состоянии покоя. */
    double GetAbsSpeed() const noexcept;
    /** @brief Меняет направление движения; пустая строка останавливает без смены ориентации. */
    void SetDirection(const std::string& direction);
    /** @brief Возвращает ориентацию U, D, L или R. */
    const std::string& GetDirection() const noexcept;
    /** @brief Помещает предмет в сумку при наличии места; иначе оставляет состояние неизменным. */
    void PutLootInBag(const Loot& loot);
    /** @brief Доставляет всю сумку и увеличивает счёт атомарно.
     * @throws std::overflow_error Итоговый счёт не помещается в int; сумка и счёт не меняются.
     */
    void ExchangeLoot();
    /** @brief Предоставляет содержимое сумки для чтения; ссылка действует до изменения собаки. */
    const std::vector<Loot>& GetLootList() const noexcept;
    /** @brief Проверяет возможность принять ещё один предмет. */
    bool HasBagSpace() const noexcept;
    /** @brief Возвращает максимальное число предметов в сумке. */
    int GetBagCapacity() const noexcept;
    /** @brief Возвращает накопленные очки за доставленные предметы. */
    int GetScore() const noexcept;
    /** @brief Возвращает длительность жизни в миллисекундах. */
    int GetPlayTime() const noexcept;
    /** @brief Возвращает непрерывное время покоя в миллисекундах. */
    int GetInactivityTime() const noexcept;
    /** @brief Проверяет допустимость шага до изменения игрового мира. */
    void ValidateTimeDelta(const int time_delta) const;
    /** @brief Обновляет счётчики времени; координаты изменяются сессией отдельно. */
    void UpdateState(const int time_delta);
    /** @brief Принимает U/D/L/R и пустую команду остановки. */
    static bool IsValidDirection(const std::string& direction);

private:
    /** @brief Отображаемое имя сущности. */
    std::string m_name{};
    /** @brief Идентификатор сущности, не изменяемый после регистрации. */
    Id m_id{0};
    /** @brief Текущая позиция в координатах карты. */
    Position m_position{};
    /** @brief Настроенный неотрицательный модуль скорости. */
    double m_abs_speed = 0.0;
    /** @brief Текущий вектор движения; нулевой вектор означает остановку. */
    geom::Vec2D m_map_speed{};
    /** @brief Ориентация U/D/L/R, сохраняемая при остановке. */
    std::string m_dir{};
    /** @brief Максимальное число предметов, неотрицательное. */
    int m_bag_capacity = 0;
    /** @brief Предметы, которыми владеет сущность. */
    std::vector<Loot> m_loot_list{};
    /** @brief Накопленные неотрицательные очки за доставку. */
    int m_score = 0;
    /** @brief Время жизни собаки в миллисекундах. */
    int m_play_time = 0;
    /** @brief Непрерывное время покоя в миллисекундах. */
    int m_inactivity_time = 0;
};
} // namespace model
