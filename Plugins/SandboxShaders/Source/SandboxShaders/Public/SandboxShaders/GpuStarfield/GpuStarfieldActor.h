#pragma once

#include "SandboxShaders/GpuStarfield/GpuStarfieldComponent.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/NoExportTypes.h"

#include "GpuStarfieldActor.generated.h"

namespace ioj {
UCLASS(Blueprintable)
class SANDBOXSHADERS_API AGpuStarfieldActor : public AActor {
    GENERATED_BODY()
  public:
    AGpuStarfieldActor();

    void PostRegisterAllComponents() override;
    void set_starfield_settings(starfield::Settings const& settings);

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "GPU Starfield")
    void load_preview_settings();

    UPROPERTY(EditAnywhere,
              Category = "GPU Starfield",
              meta = (FilePathFilter = "scm", RelativeToGameDir))
    FFilePath settings_file{TEXT("LevelScripts/Settings/base.scm")};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPU Starfield")
    FGpuStarfieldColours colours;
  protected:
    UPROPERTY(VisibleAnywhere, Category = "GPU Starfield")
    TObjectPtr<UGpuStarfieldComponent> starfield_component_;
};

} // namespace ioj
