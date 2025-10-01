#pragma once

#include <optional>
#include <tuple>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/data/pool/PoolConfig.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/utilities/actor_utils.h"

template <typename... Configs>
    requires (IsPoolConfig<Configs> && ...)
class UObjectPoolSubsystemCore : public ml::LogMsgMixin<"UObjectPoolSubsystemCore"> {
  public:
    using free_list_type = TArray<int32>;

    void initialize_pools(UWorld* world) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UObjectPoolSubsystemCore::initialize_pools"))

        if (initialized_) {
            log_warning(TEXT("Pools already initialized"));
            return;
        }

        if (!world) {
            log_error(TEXT("Cannot initialize pools without world"));
            return;
        }

        world_ = world;
        initialized_ = true;

        log_verbose(TEXT("Object pools initialized"));
    }

    template <typename Config>
    typename Config::ActorType*
        get_item(TSubclassOf<typename Config::ActorType> actor_class = nullptr) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UObjectPoolSubsystemCore::get_item"))

        static constexpr auto logger{NestedLogger<"get_item">()};

        if (!initialized_) {
            logger.log_error(TEXT("Pools not initialized"));
            return nullptr;
        }

        // Use default class if none specified
        if (!actor_class) {
            actor_class = Config::GetDefaultClass();
        }

        if (!actor_class) {
            logger.log_error(TEXT("No actor_class specified and no default available"));
            return nullptr;
        }

        auto& freelist{get_freelist<Config>(actor_class)};
        auto& pool{get_pool<Config>()};

        // Lazy spawn if free list is empty
        if (freelist.IsEmpty()) {
            if (!extend_pool<Config>(actor_class, freelist)) {
                return nullptr;
            }
        }

        // Reuse from pool
        auto const pool_idx{freelist.Pop()};
        auto* actor{pool[pool_idx].Get()};
        actor->Activate();

        logger.log_very_verbose(TEXT("Retrieved actor %s from pool at index %d"),
                                *ActorUtils::GetBestDisplayName(actor),
                                pool_idx);

        return actor;
    }

    template <typename Config>
    void return_item(typename Config::ActorType* item) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UObjectPoolSubsystemCore::return_item"))

        static constexpr auto logger{NestedLogger<"return_item">()};

        if (!item) {
            logger.log_warning(TEXT("Attempted to return null item"));
            return;
        }

        auto& pool{get_pool<Config>()};
        auto const pool_idx{pool.Find(item)};

        if (pool_idx == INDEX_NONE) {
            logger.log_error(TEXT("Attempted to return item not in pool"));
            return;
        }

        // Look up which free list this actor's class belongs to
        auto actor_class{item->GetClass()};
        auto* freelist_idx_ptr{subclass_to_index_.Find(actor_class)};

        if (!freelist_idx_ptr) {
            logger.log_error(TEXT("No free list found for actor class %s"),
                             *actor_class->GetName());
            return;
        }

        auto freelist_idx{*freelist_idx_ptr};
        auto& freelist{free_indexes_[freelist_idx]};

        // Check for double-return
        if (freelist.Contains(pool_idx)) {
            logger.log_error(TEXT("Attempted to return already-free item at index %d"), pool_idx);
            return;
        }

        item->Deactivate();
        freelist.Push(pool_idx);

        logger.log_verbose(TEXT("Returned actor to pool at index %d"), pool_idx);
    }

    template <typename Config>
    void preallocate(UWorld& world,
                     TSubclassOf<typename Config::ActorType> actor_class,
                     int32 count) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UObjectPoolSubsystemCore::preallocate"))
        add_pool_members<Config>(world, actor_class, count);
    }
  private:
    template <typename Config>
    auto& get_pool() {
        using PoolType = TArray<TObjectPtr<typename Config::ActorType>>;
        return std::get<PoolType>(pools_);
    }

    template <typename Config>
    [[nodiscard]] bool extend_pool(TSubclassOf<typename Config::ActorType> actor_class,
                                   free_list_type& free_list) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UObjectPoolSubsystemCore::extend_pool"))
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
            constexpr auto max_size{Config::MaxPoolSize.value()};
            if (target_size > max_size) {
                if (current_size >= max_size) {
                    logger.log_error(TEXT("Pool exhausted and at max capacity %d"), max_size);
                    return false;
                }
                new_items = max_size - current_size;
            }
        }

        add_pool_members<Config>(*world, actor_class, new_items);

        return true;
    }

    template <typename Config>
    void add_pool_members(UWorld& world,
                          TSubclassOf<typename Config::ActorType> actor_class,
                          int32 n) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UObjectPoolSubsystemCore::add_pool_members"))
        static constexpr auto logger{NestedLogger<"add_pool_members">()};

        if (!actor_class) {
            logger.log_error(TEXT("actor_class is nullptr"));
            return;
        }

        auto& pool{get_pool<Config>()};
        auto& free_idxs{get_freelist<Config>(actor_class)};

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
    auto& get_freelist(TSubclassOf<typename Config::ActorType> actor_class) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UObjectPoolSubsystemCore::get_freelist"))
        static constexpr auto logger{NestedLogger<"get_freelist">()};

        int32* freelist_idx_ptr{subclass_to_index_.Find(actor_class)};
        int32 freelist_idx{-1};

        if (!freelist_idx_ptr) {
            // First time seeing this actor_class, create free list
            freelist_idx = free_indexes_.Add(TArray<int32>{});
            subclass_to_index_.Add(actor_class, freelist_idx);
            logger.log_verbose(
                TEXT("Created free list %d for class %s"), freelist_idx, *actor_class->GetName());
        } else {
            freelist_idx = *freelist_idx_ptr;
        }

        return free_indexes_[freelist_idx];
    }

    std::tuple<TArray<TObjectPtr<typename Configs::ActorType>>...> pools_;
    TMap<TSubclassOf<AActor>, int32> subclass_to_index_;
    TArray<free_list_type> free_indexes_;
    TWeakObjectPtr<UWorld> world_;
    bool initialized_{false};
};
