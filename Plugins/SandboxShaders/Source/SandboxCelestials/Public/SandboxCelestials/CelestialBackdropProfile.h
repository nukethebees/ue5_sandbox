#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SandboxCelestials/CelestialBackdropActor.h"

#include "CelestialBackdropProfile.generated.h"

UCLASS(BlueprintType)
class SANDBOXCELESTIALS_API UCelestialBackdropProfile final : public UDataAsset {
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Celestial Backdrop",
              meta = (ShowOnlyInnerProperties))
    FCelestialBackdropAppearanceSettings appearance;

    void apply_to(FCelestialBackdropSettings& target) const;

#if WITH_EDITOR
    void PostEditChangeProperty(FPropertyChangedEvent& property_changed_event) override;
#endif
};
