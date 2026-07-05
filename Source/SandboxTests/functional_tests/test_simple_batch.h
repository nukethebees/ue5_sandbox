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

    void Tick(float dt) override;
  protected:
    void BeginPlay() override;
    void PrepareTest() override;
    void StartTest() override;
    void OnTimeout() override;
    void FinishTest(EFunctionalTestResult TestResult, FString const& Message) override;

    void pre_check();
    void check_alive_matches_kills();
    void update_kills();

    void handle_kill_count_reached();
    auto get_fail_message() const -> FString;
    void handle_fail();

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestStaticTurrets> turrets{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    int32 expected_kills{1};

    UPROPERTY()
    int32 kills{0};

    UPROPERTY()
    bool all_passed{true};
};
