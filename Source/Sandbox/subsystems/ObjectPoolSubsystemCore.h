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
        GetItem(TSubclassOf<typename Config::ActorType> SubClass = nullptr) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UObjectPoolSubsystemCore::GetItem"))

        static constexpr auto logger{NestedLogger<"GetItem">()};

        if (!initialized_) {
            logger.log_error(TEXT("Pools not initialized"));
            return nullptr;
        }

        // Use default class if none specified
        if (!SubClass) {
            SubClass = Config::GetDefaultClass();
        }

        if (!SubClass) {
            logger.log_error(TEXT("No subclass specified and no default available"));
            return nullptr;
        }

        // Get or create free list for this subclass
        int32* freelist_idx_ptr{subclass_to_freelist_.Find(SubClass)};
        int32 freelist_idx{-1};

        if (!freelist_idx_ptr) {
            // First time seeing this subclass, create free list
            freelist_idx = free_indexes_.Add(TArray<int32>{});
            subclass_to_freelist_.Add(SubClass, freelist_idx);
            logger.log_verbose(
                TEXT("Created free list %d for class %s"), freelist_idx, *SubClass->GetName());
        } else {
            freelist_idx = *freelist_idx_ptr;
        }

        auto& freelist{free_indexes_[freelist_idx]};
        auto& pool{get_pool<Config>()};

        // Lazy spawn if free list is empty
        if (freelist.IsEmpty()) {
            auto* actor{spawn_actor<Config>(SubClass)};
            if (!actor) {
                logger.log_error(TEXT("Failed to spawn actor of class %s"), *SubClass->GetName());
                return nullptr;
            }

            auto pool_idx{pool.Add(actor)};
            logger.log_verbose(TEXT("Lazily spawned actor %s at pool index %d"),
                               *ActorUtils::GetBestDisplayName(actor),
                               pool_idx);

            actor->Activate();
            return actor;
        }

        // Reuse from pool
        auto const pool_idx{freelist.Pop()};
        auto* actor{pool[pool_idx].Get()};

        // Handle destroyed actors
        if (!IsValid(actor)) {
            logger.log_error(TEXT("Pooled actor destroyed, reallocating at index %d"), pool_idx);
            actor = spawn_actor<Config>(SubClass);
            if (!actor) {
                logger.log_error(TEXT("Failed to respawn actor"));
                freelist.Push(pool_idx);
                return nullptr;
            }
            pool[pool_idx] = actor;
        }

        actor->Activate();
        logger.log_verbose(TEXT("Retrieved actor %s from pool at index %d"),
                           *ActorUtils::GetBestDisplayName(actor),
                           pool_idx);

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
        auto const pool_idx{pool.Find(item)};

        if (pool_idx == INDEX_NONE) {
            logger.log_error(TEXT("Attempted to return item not in pool"));
            return;
        }

        // Look up which free list this actor's class belongs to
        auto actor_class{item->GetClass()};
        auto* freelist_idx_ptr{subclass_to_freelist_.Find(actor_class)};

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
  private:
    template <typename Config>
    typename Config::ActorType* spawn_actor(TSubclassOf<typename Config::ActorType> ActorClass) {
        auto* world{world_.Get()};
        if (!world) {
            return nullptr;
        }

        if (!ActorClass) {
            return nullptr;
        }

        FActorSpawnParameters params{};
        params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        auto* actor{world->SpawnActor<typename Config::ActorType>(
            ActorClass, FVector::ZeroVector, FRotator::ZeroRotator, params)};

        if (actor) {
            actor->Activate();
            actor->Deactivate();
        }

        return actor;
    }

    template <typename Config>
    auto& get_pool() {
        using PoolType = TArray<TObjectPtr<typename Config::ActorType>>;
        return std::get<PoolType>(pools_);
    }

    std::tuple<TArray<TObjectPtr<typename Configs::ActorType>>...> pools_;
    TMap<TSubclassOf<AActor>, int32> subclass_to_freelist_;
    TArray<TArray<int32>> free_indexes_;
    TWeakObjectPtr<UWorld> world_;
    bool initialized_{false};
};
