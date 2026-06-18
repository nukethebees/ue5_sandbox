#pragma once

#include <FunctionalTest.h>

#include "test_simple_batch.generated.h"

class ATestEntityRegistry;

UCLASS()
class ATestSimpleBatch : public AFunctionalTest {
    GENERATED_BODY()
  public:
    void StartTest() override;
    void Tick(float dt) override;
  protected:
    void check_alive_matches_kills();
    void update_kills();

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    ATestEntityRegistry* entity_registry{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    int32 expected_kills{1};

    int32 kills{0};

    bool all_passed{true};
};
