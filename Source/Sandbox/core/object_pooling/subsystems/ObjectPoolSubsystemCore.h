#pragma once

#include <optional>
#include <tuple>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/core/object_pooling/concepts/ObjectPoolConcepts.h"
#include "Sandbox/core/object_pooling/data/PoolConfig.h"
#include "Sandbox/environment/utilities/actor_utils.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

template <typename... Configs>
    requires (IsPoolConfig<Configs> && ...)
class UObjectPoolSubsystemCore
    : public ml::LogMsgMixin<"UObjectPoolSubsystemCore", LogSandboxSubsystem> {
  public:
    using free_list_type = TArray<int32>;

    struct SubclassPoolData {
        TArray<int32>& freelist;
        int32& count;
        int32 index;
    };

    UObjectPoolSubsystemCore(UWorld& world)
        : world{world} {}

    template <typename Config>
    typename Config::ActorType*
        get_item(TSubclassOf<typename Config::ActorType> actor_class = nullptr) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UObjectPoolSubsystemCore::get_item"))

        static constexpr auto logger{NestedLogger<"get_item">()};

        // Use default class if none specified
        if (!actor_class) {
            actor_class = Config::GetDefaultClass();
        }

        if (!actor_class) {
            logger.log_error(TEXT("No actor_class specified and no default available"));
            return nullptr;
        }

        auto subclass_data{get_subclass_data<Config>(actor_class)};
        auto& pool{get_pool<Config>()};

        // Lazy spawn if free list is empty
        if (subclass_data.freelist.IsEmpty()) {
            logger.log_display(TEXT("Free list is empty, extending."));
            if (!extend_pool<Config>(actor_class, subclass_data.freelist)) {
                logger.log_warning(TEXT("Couldn't extend the pool."));
                return nullptr;
            }
        }

        // Reuse from pool
        auto const pool_idx{subclass_data.freelist.Pop()};
        auto* actor{pool[pool_idx].Get()};
        actor->Activate();

        logger.log_very_verbose(TEXT("Retrieved actor %s from pool at index %d (%d free)"),
                                *ml::get_best_display_name(*actor),
                                pool_idx,
                                subclass_data.freelist.Num());

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
        auto subclass_data{get_subclass_data<Config>(actor_class)};

        // Check for double-return
        if (subclass_data.freelist.Contains(pool_idx)) {
            logger.log_error(TEXT("Attempted to return already-free item at index %d"), pool_idx);
            return;
        }

        item->Deactivate();
        subclass_data.freelist.Push(pool_idx);

        logger.log_very_verbose(TEXT("Returned actor to pool at index %d. (%d free)"),
                                pool_idx,
                                subclass_data.freelist.Num());
    }

    template <typename Config>
    void preallocate(TSubclassOf<typename Config::ActorType> actor_class, int32 count) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UObjectPoolSubsystemCore::preallocate"))
        add_pool_members<Config>(actor_class, count);
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

        auto subclass_data{get_subclass_data<Config>(actor_class)};
        auto const current_size{subclass_data.count};

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
                target_size = max_size;
            }
        }

        logger.log_display(TEXT("Extending pool by %d to %d objects."), new_items, target_size);

        add_pool_members<Config>(actor_class, new_items);

        return true;
    }

    template <typename Config>
    void add_pool_members(TSubclassOf<typename Config::ActorType> actor_class, int32 n) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UObjectPoolSubsystemCore::add_pool_members"))
        static constexpr auto logger{NestedLogger<"add_pool_members">()};

        if (!actor_class) {
            logger.log_error(TEXT("actor_class is nullptr"));
            return;
        }

        auto& pool{get_pool<Config>()};
        auto subclass_data{get_subclass_data<Config>(actor_class)};

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
            subclass_data.freelist.Push(added_idx);
            ++added;
        }

        subclass_data.count += added;

        logger.log_verbose(TEXT("Added %d actors to pool"), added);
    }

    template <typename Config>
    SubclassPoolData get_subclass_data(TSubclassOf<typename Config::ActorType> actor_class) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UObjectPoolSubsystemCore::get_subclass_data"))
        static constexpr auto logger{NestedLogger<"get_subclass_data">()};

        int32* freelist_idx_ptr{subclass_to_index_.Find(actor_class)};
        int32 freelist_idx{-1};

        if (!freelist_idx_ptr) {
            // First time seeing this actor_class, create free list
            freelist_idx = free_indexes_.Add(TArray<int32>{});
            subclass_to_index_.Add(actor_class, freelist_idx);
            subclass_index_count_.Add(0);
            logger.log_verbose(
                TEXT("Created free list %d for class %s"), freelist_idx, *actor_class->GetName());
        } else {
            freelist_idx = *freelist_idx_ptr;
        }

        return SubclassPoolData{
            free_indexes_[freelist_idx], subclass_index_count_[freelist_idx], freelist_idx};
    }

    std::tuple<TArray<TObjectPtr<typename Configs::ActorType>>...> pools_;
    TMap<TSubclassOf<AActor>, int32> subclass_to_index_;
    TArray<free_list_type> free_indexes_;
    TArray<int32> subclass_index_count_;
    UWorld& world;
};
