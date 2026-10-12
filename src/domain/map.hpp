#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/tagged.hpp"

#include "domain/geometry_types.hpp"
#include "domain/loot.hpp"

namespace model
{
/** @brief Прямой участок дороги с ограниченной областью движения вокруг оси. */
class Road
{
public:
    /** @brief Метка конструктора горизонтального участка. */
    struct HorizontalTag
    {
    };
    /** @brief Метка конструктора вертикального участка. */
    struct VerticalTag
    {
    };
    /** @brief Метка выбора конструктора горизонтальной дороги. */
    static constexpr HorizontalTag HORIZONTAL{};
    /** @brief Метка выбора конструктора вертикальной дороги. */
    static constexpr VerticalTag VERTICAL{};

    /** @brief Создаёт горизонтальную дорогу; порядок концов произвольный. */
    Road(HorizontalTag, const Point start, const Coord end_x) noexcept;
    /** @brief Создаёт вертикальную дорогу; порядок концов произвольный. */
    Road(VerticalTag, const Point start, const Coord end_y) noexcept;
    /** @brief Проверяет совпадение целочисленных координат Y концов. */
    bool IsHorizontal() const noexcept;
    /** @brief Проверяет совпадение целочисленных координат X концов. */
    bool IsVertical() const noexcept;
    /** @brief Возвращает начальную точку из конфигурации. */
    Point GetStart() const noexcept;
    /** @brief Возвращает конечную точку из конфигурации. */
    Point GetEnd() const noexcept;
    /** @brief Проверяет принадлежность замкнутой области движения дороги. */
    bool IsInArea(const Position point) const;
    /** @brief Ограничивает целевую точку ближайшей границей области движения. */
    Position GetNearestPos(const Position target_pos) const;

private:
    /** @brief Полная ширина проходимой области дороги. */
    static constexpr double m_width = 0.8;
    /** @brief Начало оси дороги из конфигурации. */
    Point m_start{};
    /** @brief Конец оси дороги из конфигурации. */
    Point m_end{};
    /** @brief Минимальная координата X проходимой области. */
    double m_start_x_range = 0.0;
    /** @brief Максимальная координата X проходимой области. */
    double m_end_x_range = 0.0;
    /** @brief Минимальная координата Y проходимой области. */
    double m_start_y_range = 0.0;
    /** @brief Максимальная координата Y проходимой области. */
    double m_end_y_range = 0.0;
};

/** @brief Неподвижное прямоугольное здание, отображаемое клиентом. */
class Building
{
public:
    /** @brief Сохраняет границы здания в координатах карты. */
    explicit Building(const Rectangle bounds) noexcept;
    /** @brief Возвращает границы; ссылка действует до уничтожения здания. */
    const Rectangle& GetBounds() const noexcept;

private:
    /** @brief Границы здания, используемые представлением карты. */
    Rectangle m_bounds{};
};

/** @brief Пункт доставки предметов; позиция и смещение изображения независимы. */
class Office
{
public:
    /** @brief Строго типизированный идентификатор сущности. */
    using Id = util::Tagged<std::string, Office>;
    /** @brief Создаёт пункт доставки с идентификатором в пределах карты. */
    Office(const Id& id, const Point position, const Offset offset);
    /** @brief Возвращает идентификатор, уникальный в пределах карты. */
    const Id& GetId() const noexcept;
    /** @brief Возвращает позицию, участвующую в проверке доставки. */
    Point GetPosition() const noexcept;
    /** @brief Возвращает смещение изображения, не влияющее на физику. */
    Offset GetOffset() const noexcept;

private:
    /** @brief Идентификатор сущности, не изменяемый после регистрации. */
    Id m_id{std::string{}};
    /** @brief Текущая позиция в координатах карты. */
    Point m_position{};
    /** @brief Смещение изображения офиса относительно точки доставки. */
    Offset m_offset{};
};

/** @brief Конфигурация карты: геометрия, правила движения и каталог предметов.
 * @details После передачи в Game карта доступна только для чтения. Сессии хранят
 * невладеющую ссылку на неё; Game обеспечивает стабильность адреса.
 */
class Map
{
public:
    /** @brief Строго типизированный идентификатор сущности. */
    using Id = util::Tagged<std::string, Map>;
    /** @brief Участки дорог в порядке добавления. */
    using Roads = std::vector<Road>;
    /** @brief Здания карты в порядке добавления. */
    using Buildings = std::vector<Building>;
    /** @brief Офисы карты в порядке добавления. */
    using Offices = std::vector<Office>;

    /** @brief Создаёт конфигурацию; дороги добавляются до создания сессии. */
    Map(const Id& id, const std::string& name);
    /** @brief Возвращает идентификатор, уникальный в игре. */
    const Id& GetId() const noexcept;
    /** @brief Возвращает отображаемое имя карты. */
    const std::string& GetName() const noexcept;
    /** @brief Предоставляет здания без возможности изменить конфигурацию. */
    const Buildings& GetBuildings() const noexcept;
    /** @brief Предоставляет дороги в порядке конфигурации. */
    const Roads& GetRoads() const noexcept;
    /** @brief Предоставляет пункты доставки в порядке конфигурации. */
    const Offices& GetOffices() const noexcept;
    /** @brief Добавляет участок дороги на этапе настройки карты. */
    void AddRoad(const Road& road);
    /** @brief Добавляет здание на этапе настройки карты. */
    void AddBuilding(const Building& building);
    /** @brief Добавляет офис, сохраняя уникальность его идентификатора.
     * @throws std::invalid_argument Идентификатор уже присутствует.
     */
    void AddOffice(const Office& office);
    /** @brief Добавляет или заменяет тип предмета; разреженные номера допустимы.
     * @throws std::invalid_argument Номер типа не совпадает с параметрами или значение отрицательно.
     */
    void AddLoot(const int type, const LootProperties loot);
    /** @brief Ищет описание типа; отсутствие выражается через std::nullopt. */
    std::optional<LootProperties> GetLoot(const int type) const noexcept;
    /** @brief Выбирает существующий тип предмета, включая разреженные номера.
     * @throws std::logic_error Каталог предметов пуст.
     */
    int GetRandomLootType() const;
    /** @brief Задаёт конечную неотрицательную скорость в единицах карты в секунду. */
    void SetSpeed(const double speed);
    /** @brief Возвращает скорость движения, назначаемую новым собакам. */
    double GetSpeed() const noexcept;
    /** @brief Задаёт неотрицательную вместимость; ноль запрещает подбор предметов. */
    void SetBagCapacity(const int capacity);
    /** @brief Возвращает максимальное количество предметов в сумке. */
    int GetBagCapacity() const noexcept;
    /** @brief Возвращает число зарегистрированных типов, а не максимальный номер. */
    int GetLootTypesNum() const noexcept;
    /** @brief Выбирает целочисленную точку на оси случайной дороги.
     * @throws std::logic_error На карте нет дорог.
     */
    Position GetRandomPoint() const;

private:
    /** @brief Связь идентификатора офиса с позицией в его массиве. */
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;
    /** @brief Идентификатор сущности, не изменяемый после регистрации. */
    Id m_id{std::string{}};
    /** @brief Отображаемое имя сущности. */
    std::string m_name{};
    /** @brief Геометрия проходимых участков карты. */
    Roads m_roads{};
    /** @brief Отображаемые здания карты. */
    Buildings m_buildings{};
    /** @brief Индекс уникальных офисов в массиве m_offices. */
    OfficeIdToIndex m_office_id_to_index{};
    /** @brief Точки доставки предметов. */
    Offices m_offices{};
    /** @brief Скорость, назначаемая собакам при входе на карту. */
    double m_dog_speed = 1.0;
    /** @brief Максимальное число предметов, неотрицательное. */
    int m_bag_capacity = 3;
    /** @brief Каталог типов предметов; номера могут быть разреженными. */
    std::unordered_map<int, LootProperties> m_loot_props{};
};
} // namespace model
