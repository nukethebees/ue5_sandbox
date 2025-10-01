#pragma once

#include "CoreMinimal.h"
#include "Sandbox/data/pool/PoolConfig.h"
#include "Sandbox/subsystems/ObjectPoolSubsystemCore.h"
#include "Subsystems/WorldSubsystem.h"

#include "ObjectPoolSubsystem.generated.h"

UCLASS()
class SANDBOX_API UObjectPoolSubsystem : public UWorldSubsystem {
    GENERATED_BODY()
  public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    template <typename Config>
    typename Config::ActorType*
        GetItem(TSubclassOf<typename Config::ActorType> SubClass = nullptr) {
        return core_.GetItem<Config>(SubClass);
    }

    template <typename Config>
    void ReturnItem(typename Config::ActorType* item) {
        core_.ReturnItem<Config>(item);
    }
  private:
    UObjectPoolSubsystemCore<FBulletPoolConfig> core_{};
};
