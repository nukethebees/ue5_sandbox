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

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "GPU Starfield",
        meta = (ClampMin = "0.0",
                ClampMax = "1.0",
                ToolTip =
                    "Fraction of stars in the actor's equatorial band. Zero keeps a uniform sky."))
    float galactic_band_strength{0.65f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "GPU Starfield",
              meta = (ClampMin = "0.0",
                      ClampMax = "1.0",
                      ToolTip = "Darkens stars through the galactic midplane. Zero disables it."))
    float dust_lane_strength{0.9f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "GPU Starfield",
              meta = (ClampMin = "0.0",
                      ClampMax = "10.0",
                      ToolTip = "Adds a broad luminous band behind the stars. Zero disables it."))
    float galactic_haze_strength{0.15f};

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
              meta = (ClampMin = "0.0",
                      ClampMax = "1.0",
                      ToolTip = "Zero renders every star white."))
    float star_colour_variation_strength{0.35f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "GPU Starfield|Advanced",
              meta = (ClampMin = "0.0",
                      ClampMax = "0.1",
                      ToolTip = "Zero disables the boosted population."))
    float bright_star_fraction{0.01f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "GPU Starfield|Advanced",
              meta = (ClampMin = "1.0"))
    float bright_star_size_multiplier{1.75f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "GPU Starfield|Advanced",
              meta = (ClampMin = "1.0"))
    float bright_star_brightness_multiplier{2.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "GPU Starfield|Advanced",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float parallax_strength{0.01f};

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "GPU Starfield|Advanced",
        meta =
            (ClampMin = "0.0",
             ClampMax = "1.0",
             ToolTip =
                 "Adds a procedural cross to only the brightest stars. Zero disables the effect."))
    float bright_star_shape_strength{0.25f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "GPU Starfield|Advanced",
              meta = (ClampMin = "1.0",
                      ClampMax = "45.0",
                      Units = "deg",
                      ToolTip = "Standard deviation of the galactic band's latitude distribution."))
    float galactic_band_width_degrees{15.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "GPU Starfield|Advanced",
              meta = (ClampMin = "0.5", ClampMax = "20.0", Units = "deg"))
    float dust_lane_width_degrees{5.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "GPU Starfield|Advanced",
              meta = (ClampMin = "0.0",
                      ClampMax = "1.0",
                      ToolTip = "Varies the lane centre, width, and darkness around the sky."))
    float dust_lane_irregularity{0.7f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "GPU Starfield|Advanced",
              meta = (ClampMin = "1.0", ClampMax = "60.0", Units = "deg"))
    float galactic_haze_width_degrees{18.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPU Starfield|Advanced")
    FLinearColor galactic_haze_colour{0.18f, 0.22f, 0.35f, 1.0f};
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
