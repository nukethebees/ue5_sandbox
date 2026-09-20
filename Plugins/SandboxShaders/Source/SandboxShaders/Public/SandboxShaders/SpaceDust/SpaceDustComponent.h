#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"

#include "SpaceDustComponent.generated.h"

class UMaterialInterface;

USTRUCT(BlueprintType)
struct SANDBOXSHADERS_API FSpaceDustSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space Dust")
    bool enabled{true};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Space Dust",
              meta = (ClampMin = "0", ClampMax = "65536"))
    int32 particle_count{96};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space Dust", meta = (ClampMin = "0"))
    int32 random_seed{1337};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space Dust", meta = (ClampMin = "1.0"))
    FVector volume_dimensions{16000.0, 12000.0, 8000.0};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space Dust", meta = (ClampMin = "0.0"))
    float particle_size{8.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space Dust", meta = (ClampMin = "0.0"))
    float brightness{0.35f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space Dust")
    FLinearColor colour{0.82f, 0.9f, 1.0f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space Dust", meta = (ClampMin = "0.0"))
    float minimum_visible_speed{100.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space Dust", meta = (ClampMin = "0.0"))
    float full_visible_speed{2000.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space Dust", meta = (ClampMin = "0.0"))
    float streak_seconds{0.0125f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space Dust", meta = (ClampMin = "0.0"))
    float minimum_motion_pixels{0.75f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space Dust", meta = (ClampMin = "0.0"))
    float full_motion_pixels{4.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space Dust", meta = (ClampMin = "0.0"))
    float maximum_streak_pixels{24.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Space Dust",
              meta = (ClampMin = "0.0", ClampMax = "0.49"))
    float volume_edge_fade_fraction{0.15f};
};

[[nodiscard]] SANDBOXSHADERS_API auto normalise_space_dust_settings(FSpaceDustSettings settings)
    -> FSpaceDustSettings;
[[nodiscard]] SANDBOXSHADERS_API auto make_space_dust_translation_phase(FVector world_location,
                                                                        FVector volume_dimensions)
    -> FVector3f;

UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class SANDBOXSHADERS_API USpaceDustComponent final : public UPrimitiveComponent {
    GENERATED_BODY()
  public:
    USpaceDustComponent();

    void apply_settings(FSpaceDustSettings const& settings);
    [[nodiscard]] auto get_settings() const -> FSpaceDustSettings { return settings_; }
    void update_motion(FVector world_velocity);

    FPrimitiveSceneProxy* CreateSceneProxy() override;
    FBoxSphereBounds CalcBounds(FTransform const& local_to_world) const override;
    void SendRenderDynamicData_Concurrent() override;
    void GetUsedMaterials(TArray<UMaterialInterface*>& out_materials,
                          bool get_debug_materials = false) const override;
  private:
    void update_translation_phase();

    UPROPERTY()
    TObjectPtr<UMaterialInterface> material_;

    FSpaceDustSettings settings_;
    FVector3f world_velocity_{FVector3f::ZeroVector};
    FVector3f translation_phase_{FVector3f::ZeroVector};
};
