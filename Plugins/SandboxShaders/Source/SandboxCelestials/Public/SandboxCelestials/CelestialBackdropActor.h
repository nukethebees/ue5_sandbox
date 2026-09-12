#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CelestialBackdropActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;
class UCelestialBackdropProfile;

UENUM(BlueprintType)
enum class ECelestialBackdropStyle : uint8 {
    Rocky,
    GasGiant,
};

UENUM(BlueprintType)
enum class ECelestialBackdropEmissionPattern : uint8 {
    Noise,
    HiveCells,
    MoltenCracks,
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface|Emission")
    ECelestialBackdropEmissionPattern emission_pattern{ECelestialBackdropEmissionPattern::Noise};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Surface|Emission",
              meta = (ClampMin = "0.5", ClampMax = "64.0"))
    float emission_scale{9.0f};
};

USTRUCT(BlueprintType)
struct SANDBOXCELESTIALS_API FCelestialBackdropAccentSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accents|Polar Caps")
    bool polar_caps_enabled{false};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accents|Polar Caps")
    FLinearColor polar_cap_colour{0.65f, 0.82f, 1.0f, 1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Accents|Polar Caps",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float polar_cap_size{0.22f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Accents|Polar Caps",
              meta = (ClampMin = "0.001", ClampMax = "0.5"))
    float polar_cap_softness{0.08f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accents|Storm")
    bool storm_enabled{false};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accents|Storm")
    FLinearColor storm_colour{0.75f, 0.14f, 0.035f, 1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Accents|Storm",
              meta = (ClampMin = "-90.0", ClampMax = "90.0", Units = "deg"))
    float storm_latitude_degrees{-22.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Accents|Storm",
              meta = (ClampMin = "-180.0", ClampMax = "180.0", Units = "deg"))
    float storm_longitude_degrees{28.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Accents|Storm",
              meta = (ClampMin = "0.02", ClampMax = "0.8"))
    float storm_size{0.18f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Accents|Storm",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float storm_intensity{0.85f};
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
struct SANDBOXCELESTIALS_API FCelestialBackdropRingSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rings")
    bool enabled{false};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Rings",
              meta = (ClampMin = "1.01", ClampMax = "4.0"))
    float inner_radius_ratio{1.20f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Rings",
              meta = (ClampMin = "1.02", ClampMax = "6.0"))
    float outer_radius_ratio{2.05f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rings")
    FRotator tilt{18.0, 0.0, 12.0};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rings")
    FLinearColor inner_colour{0.10f, 0.025f, 0.004f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rings")
    FLinearColor outer_colour{0.95f, 0.35f, 0.035f, 1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Rings|Pattern",
              meta = (ClampMin = "1.0", ClampMax = "128.0"))
    float band_count{34.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Rings|Pattern",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float band_strength{0.72f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Rings|Pattern",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float breakup{0.24f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Rings|Pattern",
              meta = (ClampMin = "0.001", ClampMax = "0.25"))
    float edge_softness{0.025f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Rings",
              meta = (ClampMin = "0.0", ClampMax = "20.0"))
    float emission_intensity{1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rings|Shadow")
    bool approximate_shadow_enabled{true};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Rings|Shadow",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float shadow_darkness{0.42f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Rings|Shadow",
              meta = (ClampMin = "0.005", ClampMax = "0.5"))
    float shadow_width{0.10f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Rings|Shadow",
              meta = (ClampMin = "0.001", ClampMax = "0.25"))
    float shadow_softness{0.035f};
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
    FCelestialBackdropAccentSettings accents;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Backdrop")
    FCelestialBackdropCloudSettings clouds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Backdrop")
    FCelestialBackdropAtmosphereSettings atmosphere;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Backdrop")
    FCelestialBackdropRingSettings rings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Backdrop")
    FCelestialBackdropCloseApproachSettings close_approach;
};

USTRUCT(BlueprintType)
struct SANDBOXCELESTIALS_API FCelestialBackdropAppearanceSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Celestial Backdrop",
              meta = (ClampMin = "0.001", ClampMax = "1.0"))
    float terminator_softness{0.16f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Backdrop")
    FCelestialBackdropSurfaceSettings surface;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Backdrop")
    FCelestialBackdropAccentSettings accents;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Backdrop")
    FCelestialBackdropCloudSettings clouds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Backdrop")
    FCelestialBackdropAtmosphereSettings atmosphere;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Celestial Backdrop")
    FCelestialBackdropRingSettings rings;

    void apply_to(FCelestialBackdropSettings& target) const;
    static FCelestialBackdropAppearanceSettings
        from_settings(FCelestialBackdropSettings const& source);
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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Celestial Backdrop|Profile")
    TObjectPtr<UCelestialBackdropProfile> profile;

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Celestial Backdrop|Profile",
              meta = (EditCondition = "profile != nullptr"))
    bool override_profile_appearance{false};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Celestial Backdrop",
              meta = (ShowOnlyInnerProperties))
    FCelestialBackdropSettings settings;
  private:
    void apply_settings(FCelestialBackdropSettings const& resolved_settings);
    void ensure_materials();

    UPROPERTY(VisibleAnywhere, Category = "Celestial Backdrop")
    TObjectPtr<USceneComponent> root_;

    UPROPERTY(VisibleAnywhere, Category = "Celestial Backdrop")
    TObjectPtr<UStaticMeshComponent> surface_mesh_;

    UPROPERTY(VisibleAnywhere, Category = "Celestial Backdrop")
    TObjectPtr<UStaticMeshComponent> cloud_mesh_;

    UPROPERTY(VisibleAnywhere, Category = "Celestial Backdrop")
    TObjectPtr<UStaticMeshComponent> atmosphere_mesh_;

    UPROPERTY(VisibleAnywhere, Category = "Celestial Backdrop")
    TObjectPtr<UStaticMeshComponent> ring_mesh_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> surface_material_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> cloud_material_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> atmosphere_material_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> ring_material_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> surface_instance_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> cloud_instance_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> atmosphere_instance_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> ring_instance_;
};
