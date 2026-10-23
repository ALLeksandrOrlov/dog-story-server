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
/** @brief Item spawn interval in seconds and probability per interval. */
struct LootGeneratorConfig
{
    /** @brief Base item replenishment interval in seconds. */
    double period = 1.0;
    /** @brief Spawn probability per base interval, from 0 to 1. */
    double probability = 0.0;
};

/** @brief An isolated world for one map: dogs, items, and simulation.
 * @details The map must outlive the session. Application serializes access;
 * this class provides no internal synchronization. Copying is disabled so that
 * two worlds cannot share mutable dogs or the retirement handler.
 */
class GameSession
{
public:
    /** @brief A strongly typed entity identifier. */
    using Id = util::Tagged<std::uint32_t, GameSession>;
    /** @brief Shared ownership of a dog with the authorization registry. */
    using DogPtr = std::shared_ptr<Dog>;
    /** @brief Session dogs in registration order. */
    using Dogs = std::vector<DogPtr>;
    /** @brief Items available for collection in the game session. */
    using Loots = std::deque<Loot>;

    /** @brief Creates a world on a nonempty map with a positive retirement timeout.
     * @throws std::invalid_argument The map, generation settings, or timing settings are invalid.
     */
    explicit GameSession(const Id id, const Map* map, const LootGeneratorConfig loot_generator_config,
            const int dog_retirement_time);
    /** @brief Prevents sharing mutable entities between aggregate copies. */
    GameSession(const GameSession&) = delete;
    /** @brief Disables copying of the game entity owner. */
    GameSession& operator=(const GameSession&) = delete;
    /** @brief Moves ownership; references to the map and dogs remain valid. */
    GameSession(GameSession&&) = default;
    /** @brief Moves state; external references to the previous object must not be retained. */
    GameSession& operator=(GameSession&&) = default;
    /** @brief Returns the session's identifier within the game. */
    const Id& GetId() const noexcept;
    /** @brief Registers a dog; duplicates or allocation failures leave state unchanged. */
    void AddDog(const Dog& dog);
    /** @brief Unregisters a dog without recording a result, for example after a failed join.
     * @details Identifiers are not reused; an absent ID is ignored.
     */
    void RemoveDog(const Dog::Id& id) noexcept;
    /** @brief Returns registered dogs without allowing modification of the registry itself. */
    const Dogs& GetDogs() const;
    /** @brief Finds a dog by identifier or returns nullptr. */
    DogPtr FindDog(const Dog::Id& id) const;
    /** @brief Returns the next monotonically increasing identifier without reserving it. */
    int GetNextDogId() const noexcept;
    /** @brief Adds an item with a unique ID in the valid identifier range. */
    void AddLoot(const Loot& loot);
    /** @brief Returns a snapshot of items on the map. */
    Loots GetLoots() const;
    /** @brief Returns the next item ID without reserving it. */
    int GetNextLootId() const noexcept;
    /** @brief Returns a non-owning pointer to the immutable map configuration. */
    const Map* GetMap() const;
    /** @brief Sets the handler invoked before a dog is removed due to inactivity.
     * @details The handler must outlive Application or capture only long-lived dependencies.
     * If it throws, the dog stays in the session; the entire tick is not rolled back.
     */
    void SetDogRetirementHandler(const std::function<void(Id, Dog::Id)>& on_dog_retirement);
    /** @brief Advances time and retirement, then movement and collisions, then item generation.
     * @param time_delta Nonnegative step duration in milliseconds.
     */
    void UpdateState(const int time_delta);

private:
    /** @brief Hash function for strongly typed dog identifiers. */
    using DogIdHasher = util::TaggedHasher<Dog::Id>;
    /** @brief Dog index kept in sync with registration order. */
    using DogIdToDog = std::unordered_map<Dog::Id, DogPtr, DogIdHasher>;
    /** @brief The entity identifier, unchanged after registration. */
    Id m_id{0};
    /** @brief Non-owning pointer to a map that outlives the session. */
    const Map* m_map = nullptr;
    /** @brief World dogs in registration order. */
    Dogs m_dogs{};
    /** @brief Index of the same dogs by identifier. */
    DogIdToDog m_dog_id_to_dog{};
    /** @brief Next dog ID; removed identifiers are not reused. */
    int m_next_dog_id = 0;
    /** @brief Items owned by the entity. */
    Loots m_loot_list{};
    /** @brief Next item ID; removed identifiers are not reused. */
    int m_next_loot_id = 0;
    /** @brief Continuous idle timeout before retirement, in milliseconds. */
    int m_dog_retirement_time = 0;
    /** @brief Accumulates time between item spawns in this session. */
    loot_gen::LootGenerator m_loot_generator;
    /** @brief Saves the result and invalidates the token before removing the dog from the session. */
    std::function<void(GameSession::Id, Dog::Id)> m_on_dog_retirement{};

    /** @brief Finds a reachable point nearest the target on a road containing the initial position. */
    std::pair<double, Position> GetCloserToPoint(const Position current_pos, const Position target_pos) const;
    /** @brief Removes a dog from both indexes using a validated vector index. */
    void RemoveDogAt(const size_t index);
    /** @brief Removes collected items without throwing, preserving remaining indexes until removal. */
    void RemoveCollectedLoot(const std::vector<bool>& collected) noexcept;
    /** @brief Updates timers and retires stationary dogs. */
    void UpdateDogsState(const int time_delta);
    /** @brief Moves dogs, then processes pickups and deliveries in collision order.
     * @details Preserves completed events on error: a collected item is already removed from the map.
     * Movement and successful deliveries are not rolled back; a failed delivery preserves the bag and score.
     */
    void UpdateCollisions(const int time_delta);
    /** @brief Preserves generator state between ticks and adds new items. */
    void UpdateLoot(const int time_delta);
};
} // namespace model
