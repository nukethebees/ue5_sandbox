#include "test_simple_batch.h"

#include <Sandbox/misc/learning/test_entity_registry/TestEntityRegistry.h>

void ATestSimpleBatch::StartTest() {
    Super::StartTest();

    if (!IsValid(entity_registry)) {
        FinishTest(EFunctionalTestResult::Failed, TEXT("Entity registry was not assigned"));
        return;
    }
}
void ATestSimpleBatch::Tick(float dt) {
    Super::Tick(dt);

    auto const kills{entity_registry->get_total_kills()};
    if (kills == expected_kills) { FinishTest(EFunctionalTestResult::Succeeded, TEXT("Passed")); }

    if (kills > expected_kills) {
        FinishTest(EFunctionalTestResult::Failed, TEXT("Too many kills"));
    }
}
