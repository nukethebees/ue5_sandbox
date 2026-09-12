#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"

#include "GpuStarfieldComponent.generated.h"

class UMaterialInterface;

USTRUCT(BlueprintType)
struct SANDBOXSHADERS_API FGpuStarfieldGenerationSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Generation",
              meta = (ClampMin = "1", ClampMax = "1000000"))
    int32 star_count{10000};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    int32 random_seed{1337};
};

USTRUCT(BlueprintType)
struct SANDBOXSHADERS_API FGpuStarfieldStarSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Star",
              meta = (ClampMin = "0.001",
                      ToolTip = "Scales the logical shell distance and billboard size together."))
    float scale{1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "0.0"))
    float size_multiplier{1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "0.0"))
    float global_brightness{8.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Star",
              meta = (ClampMin = "0.0",
                      ClampMax = "1.0",
                      ToolTip = "Zero renders every star white."))
    float colour_variation_strength{0.35f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Star",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float parallax_strength{0.01f};
};

USTRUCT(BlueprintType)
struct SANDBOXSHADERS_API FGpuStarfieldGalacticBandSettings {
    GENERATED_BODY()

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Galactic Band",
        meta = (ClampMin = "0.0",
                ClampMax = "1.0",
                ToolTip =
                    "Fraction of stars in the actor's equatorial band. Zero keeps a uniform sky."))
    float strength{0.65f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Galactic Band",
              meta = (ClampMin = "1.0",
                      ClampMax = "45.0",
                      Units = "deg",
                      ToolTip = "Standard deviation of the galactic band's latitude distribution."))
    float width_degrees{15.0f};
};

USTRUCT(BlueprintType)
struct SANDBOXSHADERS_API FGpuStarfieldStellarClusterSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Stellar Cluster",
              meta = (ClampMin = "0.0",
                      ClampMax = "1.0",
                      ToolTip = "Fraction of stars concentrated into deterministic clusters."))
    float strength{0.12f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Stellar Cluster",
              meta = (ClampMin = "1.0", ClampMax = "20.0", Units = "deg"))
    float width_degrees{4.0f};
};

USTRUCT(BlueprintType)
struct SANDBOXSHADERS_API FGpuStarfieldDustLaneSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Dust Lane",
              meta = (ClampMin = "0.0",
                      ClampMax = "1.0",
                      ToolTip = "Darkens stars through the galactic midplane. Zero disables it."))
    float strength{0.9f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Dust Lane",
              meta = (ClampMin = "0.5", ClampMax = "20.0", Units = "deg"))
    float width_degrees{5.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Dust Lane",
              meta = (ClampMin = "0.0",
                      ClampMax = "1.0",
                      ToolTip = "Varies the lane centre, width, and darkness around the sky."))
    float irregularity{0.7f};
};

USTRUCT(BlueprintType)
struct SANDBOXSHADERS_API FGpuStarfieldBrightStarSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Bright Star",
              meta = (ClampMin = "0.0",
                      ClampMax = "0.1",
                      ToolTip = "Zero disables the boosted population."))
    float fraction{0.01f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Bright Star",
              meta = (ClampMin = "1.0"))
    float size_multiplier{1.75f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Bright Star",
              meta = (ClampMin = "1.0"))
    float brightness_multiplier{2.0f};

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Bright Star",
        meta =
            (ClampMin = "0.0",
             ClampMax = "1.0",
             ToolTip =
                 "Adds a procedural cross to only the brightest stars. Zero disables the effect."))
    float shape_strength{0.25f};
};

USTRUCT(BlueprintType)
struct SANDBOXSHADERS_API FGpuStarfieldTwinkleSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Twinkle",
              meta = (ClampMin = "0.0",
                      ClampMax = "0.5",
                      ToolTip = "Subtly varies the brightness of the brightest stars."))
    float strength{0.1f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Twinkle",
              meta = (ClampMin = "0.0", ClampMax = "2.0", Units = "Hz"))
    float speed{0.35f};
};

USTRUCT(BlueprintType)
struct SANDBOXSHADERS_API FGpuStarfieldGalacticHazeSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Galactic Haze",
              meta = (ClampMin = "0.0",
                      ClampMax = "10.0",
                      ToolTip = "Adds a broad luminous band behind the stars. Zero disables it."))
    float strength{0.15f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Galactic Haze",
              meta = (ClampMin = "1.0", ClampMax = "60.0", Units = "deg"))
    float width_degrees{18.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Galactic Haze")
    FLinearColor colour{0.18f, 0.22f, 0.35f, 1.0f};
};

USTRUCT(BlueprintType)
struct SANDBOXSHADERS_API FGpuStarfieldGalacticCoreSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Galactic Core",
              meta = (ClampMin = "0.0",
                      ClampMax = "10.0",
                      ToolTip = "Adds a warm core along the local positive-X galactic horizon."))
    float strength{0.65f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Galactic Core",
              meta = (ClampMin = "1.0", ClampMax = "90.0", Units = "deg"))
    float width_degrees{32.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Galactic Core")
    FLinearColor colour{0.8f, 0.42f, 0.18f, 1.0f};
};

USTRUCT(BlueprintType)
struct SANDBOXSHADERS_API FGpuStarfieldNebularKnotSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Nebular Knot",
              meta = (ClampMin = "0.0",
                      ClampMax = "10.0",
                      ToolTip = "Adds localized coloured clouds within the galactic haze."))
    float strength{0.18f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Nebular Knot",
              meta = (ClampMin = "2.0", ClampMax = "30.0", Units = "deg"))
    float size_degrees{9.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nebular Knot")
    FLinearColor colour{0.42f, 0.16f, 0.55f, 1.0f};
};

USTRUCT(BlueprintType)
struct SANDBOXSHADERS_API FGpuStarfieldSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPU Starfield")
    FGpuStarfieldGenerationSettings generation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPU Starfield")
    FGpuStarfieldStarSettings star;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPU Starfield")
    FGpuStarfieldGalacticBandSettings galactic_band;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPU Starfield")
    FGpuStarfieldStellarClusterSettings stellar_cluster;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPU Starfield")
    FGpuStarfieldDustLaneSettings dust_lane;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPU Starfield")
    FGpuStarfieldBrightStarSettings bright_star;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPU Starfield")
    FGpuStarfieldTwinkleSettings twinkle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPU Starfield")
    FGpuStarfieldGalacticHazeSettings galactic_haze;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPU Starfield")
    FGpuStarfieldGalacticCoreSettings galactic_core;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPU Starfield")
    FGpuStarfieldNebularKnotSettings nebular_knot;
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
