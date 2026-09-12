#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CelestialBackdropActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ECelestialBackdropStyle : uint8 {
    Rocky,
    GasGiant,
};

USTRUCT(BlueprintType)
struct SANDBOXCELESTIALS_API FCelestialBackdropSurfaceSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
    ECelestialBackdropStyle style{ECelestialBackdropStyle::Rocky};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
    FLinearColor primary_day_colour{0.025f, 0.12f, 0.28f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
    FLinearColor secondary_day_colour{0.07f, 0.30f, 0.12f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
    FLinearColor night_colour{0.002f, 0.006f, 0.018f, 1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Surface",
              meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float night_brightness{0.22f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Surface|Pattern",
              meta = (ClampMin = "0.1", ClampMax = "64.0"))
    float detail_scale{4.5f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Surface|Pattern",
              meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float detail_strength{1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Surface|Pattern",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float noise_breakup{0.45f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Surface|Pattern",
              meta = (ClampMin = "0", ClampMax = "12"))
    int32 stylisation_steps{0};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface|Pattern")
    int32 pattern_seed{173};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Surface|Bands",
              meta = (ClampMin = "1.0", ClampMax = "64.0"))
    float band_count{12.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Surface|Bands",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float band_strength{0.8f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Surface|Bands",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float band_warp{0.18f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface|Emission")
    FLinearColor emission_colour{0.1f, 0.55f, 1.0f, 1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Surface|Emission",
              meta = (ClampMin = "0.0", ClampMax = "50.0"))
    float emission_intensity{0.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Surface|Emission",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float emission_threshold{0.72f};
};

USTRUCT(BlueprintType)
struct SANDBOXCELESTIALS_API FCelestialBackdropCloudSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clouds")
    bool enabled{true};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clouds")
    FLinearColor colour{0.82f, 0.90f, 1.0f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clouds")
    FLinearColor night_colour{0.025f, 0.05f, 0.10f, 1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Clouds",
              meta = (ClampMin = "0.0", ClampMax = "0.25"))
    float altitude{0.018f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Clouds",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float amount{0.42f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Clouds",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float opacity{0.62f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Clouds|Pattern",
              meta = (ClampMin = "0.1", ClampMax = "64.0"))
    float detail_scale{7.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Clouds|Pattern",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float breakup{0.35f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clouds|Animation")
    float rotation_phase_degrees{0.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clouds|Animation")
    float rotation_speed_degrees_per_second{0.45f};
};

USTRUCT(BlueprintType)
struct SANDBOXCELESTIALS_API FCelestialBackdropAtmosphereSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere")
    bool enabled{true};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere")
    FLinearColor colour{0.03f, 0.36f, 1.0f, 1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Atmosphere",
              meta = (ClampMin = "0.0", ClampMax = "0.35"))
    float thickness{0.075f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Atmosphere",
              meta = (ClampMin = "0.0", ClampMax = "8.0"))
    float density{1.55f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Atmosphere",
              meta = (ClampMin = "0.0", ClampMax = "50.0"))
    float limb_intensity{7.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Atmosphere",
              meta = (ClampMin = "0.5", ClampMax = "12.0"))
    float limb_falloff{3.2f};
};

USTRUCT(BlueprintType)
struct SANDBOXCELESTIALS_API FCelestialBackdropCloseApproachSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Close Approach")
    bool enabled{true};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Close Approach",
              meta = (ClampMin = "1.05", ClampMax = "4.0"))
    float minimum_camera_distance_ratio{1.5f};
};

USTRUCT(BlueprintType)
struct SANDBOXCELESTIALS_API FCelestialBackdropSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Celestial Backdrop",
              meta = (ClampMin = "1.0", Units = "cm"))
    float body_radius{50000.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Backdrop")
    FVector sun_direction{0.35, -0.45, 0.82};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Celestial Backdrop",
              meta = (ClampMin = "0.001", ClampMax = "1.0"))
    float terminator_softness{0.16f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Backdrop")
    FCelestialBackdropSurfaceSettings surface;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Backdrop")
    FCelestialBackdropCloudSettings clouds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Backdrop")
    FCelestialBackdropAtmosphereSettings atmosphere;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Backdrop")
    FCelestialBackdropCloseApproachSettings close_approach;
};

UCLASS(Blueprintable, ClassGroup = (Rendering))
class SANDBOXCELESTIALS_API ACelestialBackdropActor final : public AActor {
    GENERATED_BODY()
  public:
    ACelestialBackdropActor();

    void OnConstruction(FTransform const& transform) override;
    void PostRegisterAllComponents() override;

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Celestial Backdrop")
    void apply_settings();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Celestial Backdrop|Presets")
    void apply_earth_like_preset();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Celestial Backdrop|Presets")
    void apply_hive_world_preset();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Celestial Backdrop|Presets")
    void apply_dark_alien_preset();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Celestial Backdrop|Presets")
    void apply_gas_giant_preset();

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Celestial Backdrop",
              meta = (ShowOnlyInnerProperties))
    FCelestialBackdropSettings settings;
  private:
    void ensure_materials();

    UPROPERTY(VisibleAnywhere, Category = "Celestial Backdrop")
    TObjectPtr<USceneComponent> root_;

    UPROPERTY(VisibleAnywhere, Category = "Celestial Backdrop")
    TObjectPtr<UStaticMeshComponent> surface_mesh_;

    UPROPERTY(VisibleAnywhere, Category = "Celestial Backdrop")
    TObjectPtr<UStaticMeshComponent> cloud_mesh_;

    UPROPERTY(VisibleAnywhere, Category = "Celestial Backdrop")
    TObjectPtr<UStaticMeshComponent> atmosphere_mesh_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> surface_material_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> cloud_material_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> atmosphere_material_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> surface_instance_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> cloud_instance_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> atmosphere_instance_;
};
