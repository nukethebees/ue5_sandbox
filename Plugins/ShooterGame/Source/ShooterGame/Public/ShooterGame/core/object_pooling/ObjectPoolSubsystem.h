#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "ShooterGame/combat/bullets/BulletPoolConfig.h"
#include "ShooterGame/core/object_pooling/ObjectPoolSubsystemCore.h"
#include "SandboxGameShared/logging/LogMsgMixin.hpp"
#include "ShooterGame/logging/ShooterGameLogCategories.h"
#include "ShooterGame/pathfinding/DroppableWaypointPoolConfig.h"

#include "ObjectPoolSubsystem.generated.h"

UCLASS()
class SHOOTERGAME_API UObjectPoolSubsystem
    : public UWorldSubsystem
    , public ml::LogMsgMixin<"UObjectPoolSubsystem", LogShooterGameSubsystem> {
    GENERATED_BODY()
  public:
    using pool_type = UObjectPoolSubsystemCore<FBulletPoolConfig, FDroppableWaypointPoolConfig>;

    UObjectPoolSubsystem()
        : core_(MakeUnique<pool_type>(*GetWorld())) {}

    template <typename Config>
    typename Config::ActorType*
        get_item(TSubclassOf<typename Config::ActorType> subclass = nullptr) {
        return core_->get_item<Config>(subclass);
    }

    template <typename Config>
    [[nodiscard]] auto return_item(typename Config::ActorType* item) {
        return core_->return_item<Config>(item);
    }

    template <typename Config>
    void preallocate(TSubclassOf<typename Config::ActorType> actor_class, int32 n) {
        core_->preallocate<Config>(actor_class, n);
    }
  private:
    TUniquePtr<pool_type> core_{nullptr};
};
