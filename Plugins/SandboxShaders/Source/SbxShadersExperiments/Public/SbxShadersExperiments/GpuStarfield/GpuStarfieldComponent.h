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
              meta = (ClampMin = "1.0"))
    float distribution_radius{50000.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "GPU Starfield",
              meta = (ClampMin = "0.0"))
    float global_star_size{100.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "GPU Starfield",
              meta = (ClampMin = "0.0"))
    float global_brightness{8.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "GPU Starfield",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float parallax_scale{0.25f};
};

struct FGpuStarfieldGpuData {
    FVector3f position{FVector3f::ZeroVector};
    float size{1.0f};
    float brightness{1.0f};
    float padding[3]{0.0f, 0.0f, 0.0f};
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
