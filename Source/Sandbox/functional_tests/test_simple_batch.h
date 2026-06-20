#pragma once

#include <FunctionalTest.h>

#include "test_simple_batch.generated.h"

class ATestEntityRegistry;
class ATestStaticTurrets;

UCLASS()
class ATestSimpleBatch : public AFunctionalTest {
    GENERATED_BODY()
  public:
    ATestSimpleBatch();

    void StartTest() override;
    void Tick(float dt) override;
  protected:
    void check_alive_matches_kills();
    void update_kills();
    
    void handle_kill_count_reached();
    void handle_fail();

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    ATestEntityRegistry* entity_registry{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    ATestStaticTurrets* turrets{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    int32 expected_kills{1};

    int32 kills{0};

    bool all_passed{true};
};
