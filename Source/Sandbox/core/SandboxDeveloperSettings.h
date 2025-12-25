#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "SandboxDeveloperSettings.generated.h"

UCLASS(Config = Game, DefaultConfig)
class USandboxDeveloperSettings : public UDeveloperSettings {
    GENERATED_BODY()
  public:
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AI")
    bool visualise_ai_vision_cones{false};
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "AI")
    bool visualise_all_patrol_paths{true};

    USandboxDeveloperSettings();
};
