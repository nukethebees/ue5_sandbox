#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>

#include <CoreMinimal.h>
#include <UObject/Interface.h>

#include "TestEntity.generated.h"

UINTERFACE()
class UTestEntity : public UInterface {
    GENERATED_BODY()
};

class ITestEntity {
    GENERATED_BODY()
  public:
    virtual auto get_entity_handle() const noexcept -> FRegistryEntityHandle = 0;
};
