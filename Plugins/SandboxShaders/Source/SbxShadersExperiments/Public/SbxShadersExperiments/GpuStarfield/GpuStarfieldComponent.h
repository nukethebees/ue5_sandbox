#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"

#include "GpuStarfieldComponent.generated.h"

class UMaterialInterface;

USTRUCT(BlueprintType)
struct SBXSHADERSEXPERIMENTS_API FGpuStarfieldSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "GPU Starfield",
              meta = (ClampMin = "1", ClampMax = "1000000"))
    int32 star_count{10000};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPU Starfield")
    int32 random_seed{1337};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "GPU Starfield",
              meta = (ClampMin = "0.001",
                      ToolTip = "Scales the logical shell distance and billboard size together."))
    float starfield_scale{1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "GPU Starfield|Advanced",
              meta = (ClampMin = "0.0"))
    float star_size_multiplier{1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "GPU Starfield|Advanced",
              meta = (ClampMin = "0.0"))
    float global_brightness{8.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "GPU Starfield|Advanced",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float parallax_strength{0.01f};
};

struct FGpuStarfieldGpuData {
    FVector3f direction{FVector3f::ZeroVector};
    float size{1.0f};
    float brightness{1.0f};
    float depth_factor{1.0f};
    float colour_temperature{0.5f};
    float padding{0.0f};
};

static_assert(sizeof(FGpuStarfieldGpuData) == 32);

UCLASS(ClassGroup = (Rendering))
class SBXSHADERSEXPERIMENTS_API UGpuStarfieldComponent final : public UPrimitiveComponent {
    GENERATED_BODY()
  public:
    UGpuStarfieldComponent();

    void apply_settings(FGpuStarfieldSettings const& settings);

    FPrimitiveSceneProxy* CreateSceneProxy() override;
    FBoxSphereBounds CalcBounds(FTransform const& local_to_world) const override;
    void SendRenderDynamicData_Concurrent() override;
    void GetUsedMaterials(TArray<UMaterialInterface*>& out_materials,
                          bool get_debug_materials = false) const override;
  private:
    void generate_stars();

    UPROPERTY()
    TObjectPtr<UMaterialInterface> material_;

    FGpuStarfieldSettings settings_;
    TArray<FGpuStarfieldGpuData> star_data_;
    bool has_generated_stars_{false};
};
