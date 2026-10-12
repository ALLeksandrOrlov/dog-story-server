#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "domain/dog.hpp"
#include "domain/loot_generator.hpp"
#include "domain/map.hpp"

namespace model
{
/** @brief Период появления предметов в секундах и вероятность на один период. */
struct LootGeneratorConfig
{
    /** @brief Базовый период пополнения предметов в секундах. */
    double period = 1.0;
    /** @brief Вероятность появления за базовый период, от 0 до 1. */
    double probability = 0.0;
};

/** @brief Изолированный игровой мир одной карты: собаки, предметы и симуляция.
 * @details Карта должна существовать дольше сессии. Доступ сериализует Application;
 * класс не обеспечивает внутреннюю синхронизацию. Копирование запрещено, чтобы
 * два мира не разделяли изменяемых собак и обработчик завершения игры.
 */
class GameSession
{
public:
    /** @brief Строго типизированный идентификатор сущности. */
    using Id = util::Tagged<std::uint32_t, GameSession>;
    /** @brief Разделяемое владение собакой с реестром авторизации. */
    using DogPtr = std::shared_ptr<Dog>;
    /** @brief Собаки сессии в порядке регистрации. */
    using Dogs = std::vector<DogPtr>;
    /** @brief Предметы, доступные для подбора в игровой сессии. */
    using Loots = std::deque<Loot>;

    /** @brief Создаёт мир на непустой карте с положительным временем выхода.
     * @throws std::invalid_argument Карта или параметры генерации/времени недопустимы.
     */
    explicit GameSession(const Id id, const Map* map, const LootGeneratorConfig loot_generator_config,
            const int dog_retirement_time);
    /** @brief Запрещает разделение изменяемых сущностей между копиями агрегата. */
    GameSession(const GameSession&) = delete;
    /** @brief Запрещает копирование владельца игровых сущностей. */
    GameSession& operator=(const GameSession&) = delete;
    /** @brief Переносит владение; ссылки на карту и собак сохраняются. */
    GameSession(GameSession&&) = default;
    /** @brief Переносит состояние; внешние ссылки на прежний объект нельзя сохранять. */
    GameSession& operator=(GameSession&&) = default;
    /** @brief Возвращает идентификатор сессии в игре. */
    const Id& GetId() const noexcept;
    /** @brief Регистрирует собаку, не изменяя состояние при дубликате или ошибке выделения. */
    void AddDog(const Dog& dog);
    /** @brief Удаляет регистрацию без начисления рекорда, например при неуспешном входе.
     * @details Идентификаторы не переиспользуются; отсутствующий ID игнорируется.
     */
    void RemoveDog(const Dog::Id& id) noexcept;
    /** @brief Возвращает зарегистрированных собак без доступа к изменению самого реестра. */
    const Dogs& GetDogs() const;
    /** @brief Ищет собаку по идентификатору или возвращает nullptr. */
    DogPtr FindDog(const Dog::Id& id) const;
    /** @brief Возвращает следующий монотонный идентификатор, не резервируя его. */
    int GetNextDogId() const noexcept;
    /** @brief Добавляет предмет с уникальным ID и допустимым диапазоном идентификатора. */
    void AddLoot(const Loot& loot);
    /** @brief Возвращает снимок предметов, лежащих на карте. */
    Loots GetLoots() const;
    /** @brief Возвращает следующий ID предмета, не резервируя его. */
    int GetNextLootId() const noexcept;
    /** @brief Возвращает невладеющий указатель на неизменяемую конфигурацию карты. */
    const Map* GetMap() const;
    /** @brief Назначает обработчик перед удалением собаки за бездействие.
     * @details Обработчик должен пережить Application либо захватывать только долгоживущие зависимости.
     * При его исключении собака остаётся в сессии; сам такт целиком не откатывается.
     */
    void SetDogRetirementHandler(const std::function<void(Id, Dog::Id)>& on_dog_retirement);
    /** @brief Выполняет шаг: время/выход, движение/столкновения, затем появление предметов.
     * @param time_delta Неотрицательная длительность шага в миллисекундах.
     */
    void UpdateState(const int time_delta);

private:
    /** @brief Хеширование типизированного идентификатора собаки. */
    using DogIdHasher = util::TaggedHasher<Dog::Id>;
    /** @brief Индекс собак, синхронизированный с последовательностью регистрации. */
    using DogIdToDog = std::unordered_map<Dog::Id, DogPtr, DogIdHasher>;
    /** @brief Идентификатор сущности, не изменяемый после регистрации. */
    Id m_id{0};
    /** @brief Невладеющий указатель на карту, переживающую сессию. */
    const Map* m_map = nullptr;
    /** @brief Собаки мира в порядке регистрации. */
    Dogs m_dogs{};
    /** @brief Индекс тех же собак по идентификатору. */
    DogIdToDog m_dog_id_to_dog{};
    /** @brief Следующий ID собаки; удалённые номера не переиспользуются. */
    int m_next_dog_id = 0;
    /** @brief Предметы, которыми владеет сущность. */
    Loots m_loot_list{};
    /** @brief Следующий ID предмета; удалённые номера не переиспользуются. */
    int m_next_loot_id = 0;
    /** @brief Срок непрерывного бездействия до выхода, в миллисекундах. */
    int m_dog_retirement_time = 0;
    /** @brief Накопитель времени появления предметов данной сессии. */
    loot_gen::LootGenerator m_loot_generator;
    /** @brief Сохраняет результат и аннулирует токен до удаления собаки из сессии. */
    std::function<void(GameSession::Id, Dog::Id)> m_on_dog_retirement{};

    /** @brief Находит достижимую точку ближайшей к цели дороги, содержащей исходную позицию. */
    std::pair<double, Position> GetCloserToPoint(const Position current_pos, const Position target_pos) const;
    /** @brief Удаляет собаку из обоих индексов по проверенному индексу вектора. */
    void RemoveDogAt(const size_t index);
    /** @brief Удаляет успешно подобранные предметы без исключений, сохраняя индексы оставшихся до удаления. */
    void RemoveCollectedLoot(const std::vector<bool>& collected) noexcept;
    /** @brief Обновляет таймеры и завершает игры неподвижных собак. */
    void UpdateDogsState(const int time_delta);
    /** @brief Перемещает собак, затем обрабатывает подбор и доставку по времени столкновений.
     * @details При ошибке сохраняет завершённые события: подобранный предмет уже отсутствует на карте.
     * Общего отката движения и успешных доставок нет; неуспешная доставка сохраняет сумку и счёт.
     */
    void UpdateCollisions(const int time_delta);
    /** @brief Сохраняет состояние генератора между тактами и добавляет новые предметы. */
    void UpdateLoot(const int time_delta);
};
} // namespace model
