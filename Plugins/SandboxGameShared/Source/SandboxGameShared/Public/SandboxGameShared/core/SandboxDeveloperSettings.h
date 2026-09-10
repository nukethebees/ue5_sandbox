#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "SandboxDeveloperSettings.generated.h"

UCLASS(Config = Game, DefaultConfig)
class SANDBOXGAMESHARED_API USandboxDeveloperSettings : public UDeveloperSettings {
    GENERATED_BODY()
  public:
    USandboxDeveloperSettings();

    auto get_effective_max_live_fighters() const noexcept -> int32;

    inline static constexpr int32 minimum_max_live_fighters{1000};

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AI")
    bool visualise_ai_vision_cones{false};
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AI")
    bool visualise_all_patrol_paths{true};

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Collision")
    bool show_collision{true};
#endif

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    bool pause_pie_on_start{false};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    bool print_save_data{false};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    bool log_successful_assertions{false};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    bool export_test_results{false};

    UPROPERTY(Config,
              EditAnywhere,
              Category = "Space Game|Simulation",
              meta = (ClampMin = "100", UIMin = "100"))
    int32 max_live_fighters{2000};
};
