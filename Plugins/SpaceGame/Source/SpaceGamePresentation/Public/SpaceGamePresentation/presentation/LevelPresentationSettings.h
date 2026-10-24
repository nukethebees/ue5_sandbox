#pragma once

#include <ioj/settings/entity_overlay.h>
#include <ioj/settings/radar.h>

#include "LevelPresentationSettings.generated.h"

class UMaterialInterface;
class UStaticMesh;

USTRUCT(BlueprintType)
struct FSoftTargetSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Rendering")
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Rendering")
    TObjectPtr<UMaterialInterface> material{nullptr};
};

USTRUCT(BlueprintType)
struct FEntityOverlaySettings {
    GENERATED_BODY()

    ioj::settings::EntityOverlay values;

    UPROPERTY(EditAnywhere, Category = "Entity Overlay")
    FSoftTargetSettings soft_target;

    UPROPERTY(EditAnywhere, Category = "Entity Overlay")
    FLinearColor background_color{0.02f, 0.02f, 0.02f, 0.85f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay")
    FLinearColor fill_color{0.10f, 0.85f, 0.20f, 1.0f};
};
