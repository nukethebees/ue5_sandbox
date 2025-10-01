#pragma once

#include <optional>
#include <tuple>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/data/pool/PoolConfig.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/utilities/actor_utils.h"
#include "Sandbox/utilities/tuple.h"

namespace ml {
template <typename T>
using ObjectPool = TArray<TObjectPtr<T>>;
}

template <typename... Configs>
    requires (IsPoolConfig<Configs> && ...)
class UObjectPoolSubsystemCore : public ml::LogMsgMixin<"UObjectPoolSubsystemCore"> {
  public:
    using pools_type = std::tuple<ml::ObjectPool<typename Configs::ActorType>...>;
    using index_type = int32;
    using indexes_type = TArray<index_type>;

    static constexpr auto n_configs{sizeof...(Configs)};

    UObjectPoolSubsystemCore() {
        constexpr auto n{static_cast<int32>(n_configs)};
        free_indexes_.SetNum(n);
    }

    void initialize_pools(UWorld& world, TArray<TSubclassOf<AActor>>&& subclasses) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UObjectPoolSubsystemCore::initialize_pools"))

        constexpr auto expected_n_subclasses{static_cast<int32>(n_configs)};
        auto const n_subclasses{subclasses.Num()};
        if (expected_n_subclasses != n_subclasses) {
            log_warning(
                TEXT("Expected %d subclasses, got %d."), expected_n_subclasses, n_subclasses);
            return;
        }

        subclasses_ = std::move(subclasses);

        if (initialized_) {
            log_warning(TEXT("Pools already initialized"));
            return;
        }

        world_ = &world;
        (initialize_pool<Configs>(world), ...);
        initialized_ = true;

        log_verbose(TEXT("Object pools initialized"));
    }

    template <typename Config>
    typename Config::ActorType* GetItem() {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UObjectPoolSubsystemCore::GetItem"))

        static constexpr auto logger{NestedLogger<"GetItem">()};

        if (!initialized_) {
            logger.log_error(TEXT("Pools not initialized"));
            return nullptr;
        }

        auto& free_idxs{get_free_indexes<Config>()};

        // Extend pool if empty
        if (free_idxs.IsEmpty()) {
            if (!extend_pool<Config>()) {
                logger.log_error(TEXT("Failed to extend pool, max capacity reached"));
                return nullptr;
            }
        }

        auto const index{free_idxs.Pop()};
        auto& pool{get_pool<Config>()};
        auto* actor{pool[index].Get()};

        // Handle destroyed actors
        if (!IsValid(actor)) {
            logger.log_error(TEXT("Pooled actor destroyed, reallocating at index %d"), index);
            actor = spawn_actor<Config>();
            if (!actor) {
                logger.log_error(TEXT("Failed to respawn actor"));
                free_idxs.Push(index);
                return nullptr;
            }
            pool[index] = actor;
        }

        actor->Activate();
        logger.log_verbose(TEXT("Retrieved actor %s from pool at index %d"),
                           *ActorUtils::GetBestDisplayName(actor),
                           index);

        return actor;
    }

    template <typename Config>
    void ReturnItem(typename Config::ActorType* item) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UObjectPoolSubsystemCore::ReturnItem"))

        static constexpr auto logger{NestedLogger<"ReturnItem">()};

        if (!item) {
            logger.log_warning(TEXT("Attempted to return null item"));
            return;
        }

        auto& pool{get_pool<Config>()};
        auto const index{pool.Find(item)};

        if (index == INDEX_NONE) {
            logger.log_error(TEXT("Attempted to return item not in pool"));
            return;
        }

        auto& free_idxs{get_free_indexes<Config>()};

        // Check for double-return
        if (free_idxs.Contains(index)) {
            logger.log_error(TEXT("Attempted to return already-free item at index %d"), index);
            return;
        }

        item->Deactivate();
        free_idxs.Push(index);

        logger.log_verbose(TEXT("Returned actor to pool at index %d"), index);
    }
  private:
    template <typename Config>
    void initialize_pool(UWorld& world) {
        static constexpr auto logger{NestedLogger<"initialize_pool">()};

        add_pool_members<Config>(world, Config::DefaultPoolSize);
        logger.log_verbose(TEXT("Initialized pool."));
    }

    template <typename Config>
    bool extend_pool() {
        static constexpr auto logger{NestedLogger<"extend_pool">()};

        auto* world{world_.Get()};
        if (!world) {
            logger.log_error(TEXT("World is invalid"));
            return false;
        }

        auto& pool{get_pool<Config>()};
        auto const current_size{pool.Num()};

        // Calculate 1.5x growth
        auto new_items{FMath::Max(1, current_size / 2)};
        auto target_size{current_size + new_items};

        // Check max pool size constraint
        if constexpr (Config::MaxPoolSize.has_value()) {
            auto const max_size{Config::MaxPoolSize.value()};
            if (target_size > max_size) {
                if (current_size >= max_size) {
                    logger.log_error(TEXT("Pool exhausted and at max capacity %d"), max_size);
                    return false;
                }
                new_items = max_size - current_size;
            }
        }

        add_pool_members<Config>(*world, Config::DefaultPoolSize);

        return true;
    }

    template <typename Config>
    void add_pool_members(UWorld& world, int32 n) {
        static constexpr auto logger{NestedLogger<"initialize_pool">()};

        auto actor_class{Config::GetDefaultClass()};
        if (!actor_class) {
            logger.log_error(TEXT("Actor class is invalid"));
            return;
        }

        auto& pool{get_pool<Config>()};
        auto& free_idxs{get_free_indexes<Config>()};

        FActorSpawnParameters params{};
        params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        int added{0};
        for (int32 i{0}; i < n; ++i) {
            auto* actor{world.SpawnActor<typename Config::ActorType>(
                actor_class, FVector::ZeroVector, FRotator::ZeroRotator, params)};

            if (!actor) {
                logger.log_error(TEXT("Failed to spawn pooled actor at index %d"), i);
                continue;
            }

            // Initialize with activate/deactivate cycle
            actor->Activate();
            actor->Deactivate();

            auto const added_idx{pool.Add(actor)};
            free_idxs.Push(added_idx);
            ++added;
        }

        logger.log_verbose(TEXT("Added %d actors to pool"), added);
    }

    template <typename Config>
    typename Config::ActorType* spawn_actor() {
        auto* world{world_.Get()};
        if (!world) {
            return nullptr;
        }

        auto actor_class{Config::GetDefaultClass()};
        if (!actor_class) {
            return nullptr;
        }

        FActorSpawnParameters params{};
        params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        auto* actor{world->SpawnActor<typename Config::ActorType>(
            actor_class, FVector::ZeroVector, FRotator::ZeroRotator, params)};

        if (actor) {
            actor->Activate();
            actor->Deactivate();
        }

        return actor;
    }

    template <typename Config>
    auto& get_pool() {
        using PoolType = ml::ObjectPool<typename Config::ActorType>;
        return std::get<PoolType>(pools_);
    }

    template <typename Config>
    auto& get_free_indexes() {
        using PoolType = ml::ObjectPool<typename Config::ActorType>;
        constexpr auto pool_index{ml::tuple_type_index_v<PoolType, pools_type>};
        return free_indexes_[pool_index];
    }

    pools_type pools_;
    TArray<TSubclassOf<AActor>> subclasses_;
    TArray<indexes_type> free_indexes_;
    TWeakObjectPtr<UWorld> world_;
    bool initialized_{false};
};
