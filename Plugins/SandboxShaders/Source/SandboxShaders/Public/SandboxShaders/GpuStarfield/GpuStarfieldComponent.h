#pragma once

#include <ioj/gfx/starfield_settings.h>

#include "Components/PrimitiveComponent.h"
#include "CoreMinimal.h"

#include "GpuStarfieldComponent.generated.h"

class UMaterialInterface;

USTRUCT(BlueprintType)
struct SANDBOXSHADERS_API FGpuStarfieldColours {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPU Starfield")
    FLinearColor galactic_haze{0.18f, 0.22f, 0.35f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPU Starfield")
    FLinearColor galactic_core{0.8f, 0.42f, 0.18f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPU Starfield")
    FLinearColor nebular_knot{0.42f, 0.16f, 0.55f, 1.0f};
};

struct FGpuStarfieldGpuData {
    FVector3f direction{FVector3f::ZeroVector};
    float size{1.0f};
    float brightness{1.0f};
    float depth_factor{1.0f};
    float colour_temperature{0.5f};
    float bright_star_factor{0.0f};
};

static_assert(sizeof(FGpuStarfieldGpuData) == 32);

UCLASS(ClassGroup = (Rendering))
class SANDBOXSHADERS_API UGpuStarfieldComponent final : public UPrimitiveComponent {
    GENERATED_BODY()
  public:
    UGpuStarfieldComponent();

    // Settings must come from the validated native starfield resolver.
    void apply_settings(ioj::starfield::Settings const& settings,
                        FGpuStarfieldColours const& colours);

    FPrimitiveSceneProxy* CreateSceneProxy() override;
    FBoxSphereBounds CalcBounds(FTransform const& local_to_world) const override;
    void SendRenderDynamicData_Concurrent() override;
    void GetUsedMaterials(TArray<UMaterialInterface*>& out_materials,
                          bool get_debug_materials = false) const override;
  private:
    void generate_stars();

    UPROPERTY()
    TObjectPtr<UMaterialInterface> material_;

    ioj::starfield::Settings settings_;
    FGpuStarfieldColours colours_;
    TArray<FGpuStarfieldGpuData> star_data_;
    bool has_generated_stars_{false};
};
