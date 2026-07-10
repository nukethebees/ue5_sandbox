#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

#include "TestEntityProxy.generated.h"

UCLASS()
class SANDBOX_API ATestEntityProxy : public AActor {
    GENERATED_BODY()
  public:
    auto get_entity_handle() const noexcept { return entity_handle; }
    void set_entity_handle(FRegistryEntityHandle const h) noexcept { entity_handle = h; }
  private:
    FRegistryEntityHandle entity_handle{};
};
