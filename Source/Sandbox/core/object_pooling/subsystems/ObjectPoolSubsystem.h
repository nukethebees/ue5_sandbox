#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Sandbox/core/object_pooling/data/PoolConfig.h"
#include "Sandbox/core/object_pooling/subsystems/ObjectPoolSubsystemCore.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "ObjectPoolSubsystem.generated.h"

UCLASS()
class SANDBOX_API UObjectPoolSubsystem
    : public UWorldSubsystem
    , public ml::LogMsgMixin<"UObjectPoolSubsystem", LogSandboxSubsystem> {
    GENERATED_BODY()
  public:
    using pool_type = UObjectPoolSubsystemCore<FBulletPoolConfig>;

    UObjectPoolSubsystem()
        : core_(MakeUnique<pool_type>(*GetWorld())) {}

    template <typename Config>
    typename Config::ActorType*
        get_item(TSubclassOf<typename Config::ActorType> subclass = nullptr) {
        return core_->get_item<Config>(subclass);
    }

    template <typename Config>
    void return_item(typename Config::ActorType* item) {
        core_->return_item<Config>(item);
    }

    template <typename Config>
    void preallocate(TSubclassOf<typename Config::ActorType> actor_class, int32 n) {
        core_->preallocate<Config>(actor_class, n);
    }
  private:
    TUniquePtr<pool_type> core_{nullptr};
};
