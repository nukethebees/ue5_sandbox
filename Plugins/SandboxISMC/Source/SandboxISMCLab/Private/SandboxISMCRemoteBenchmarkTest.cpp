#include "Editor/UnrealEdEngine.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "UnrealEdGlobals.h"

#if WITH_AUTOMATION_TESTS

namespace {
class FWaitForSandboxISMCBenchmark final : public IAutomationLatentCommand {
  public:
    explicit FWaitForSandboxISMCBenchmark(FAutomationTestBase& test)
        : test_{test}
        , start_seconds_{FPlatformTime::Seconds()} {}

    virtual bool Update() override {
        if (GUnrealEd != nullptr && IsValid(GUnrealEd->PlayWorld)) {
            saw_play_world_ = true;
            return false;
        }

        if (saw_play_world_) {
            return true;
        }

        constexpr double start_timeout_seconds{120.0};
        if (FPlatformTime::Seconds() - start_seconds_ > start_timeout_seconds) {
            test_.AddError(TEXT("SandboxISMC benchmark PIE session did not start"));
            return true;
        }
        return false;
    }
  private:
    FAutomationTestBase& test_;
    double start_seconds_{0.0};
    bool saw_play_world_{false};
};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandboxISMCRemoteBenchmarkTest,
                                 "SandboxISMC.RemoteBenchmark",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FSandboxISMCRemoteBenchmarkTest::RunTest(FString const& parameters) {
    if (GUnrealEd == nullptr) {
        AddError(TEXT("GUnrealEd is not available"));
        return false;
    }

    FString load_error;
    GUnrealEd->AutomationLoadMap(
        TEXT("/SandboxISMC/Lab/FT_SandboxISMCBenchmark"), true, &load_error);
    if (!load_error.IsEmpty()) {
        AddError(
            FString::Printf(TEXT("Could not load SandboxISMC benchmark map: %s"), *load_error));
        return false;
    }

    ADD_LATENT_AUTOMATION_COMMAND(FWaitForSandboxISMCBenchmark(*this));
    return true;
}

#endif
