#pragma once

#include <ioj/sim/entity_types.h>

#include <CoreMinimal.h>
#include <UObject/Interface.h>

#include "TestEntity.generated.h"

UINTERFACE(MinimalAPI)
class UTestEntity : public UInterface {
    GENERATED_BODY()
};

class ITestEntity {
    GENERATED_BODY()
  public:
    virtual auto get_unique_id() const noexcept -> ::ioj::sim::EntityUniqueId = 0;
#if WITH_EDITOR
    virtual auto get_test_name() const noexcept -> FName = 0;
#endif
};
