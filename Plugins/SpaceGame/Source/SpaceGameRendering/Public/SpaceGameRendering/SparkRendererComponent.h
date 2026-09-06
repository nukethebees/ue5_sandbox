#pragma once

#include <Components/PrimitiveComponent.h>
#include <CoreMinimal.h>

#include "SparkRendererComponent.generated.h"

USTRUCT(BlueprintType)
struct SPACEGAMERENDERING_API FSparkScalarRange {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Sparks")
    float min{0.0f};

    UPROPERTY(EditAnywhere, Category = "Sparks")
    float max{0.0f};
};

USTRUCT(BlueprintType)
struct SPACEGAMERENDERING_API FSparkBurstStyle {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Sparks", meta = (ClampMin = "0"))
    int32 count{16};

    UPROPERTY(EditAnywhere, Category = "Sparks")
    FSparkScalarRange speed{3000.0f, 10000.0f};

    UPROPERTY(EditAnywhere, Category = "Sparks")
    FSparkScalarRange lifetime{0.08f, 0.25f};

    UPROPERTY(EditAnywhere, Category = "Sparks")
    FSparkScalarRange size{4.0f, 12.0f};

    UPROPERTY(EditAnywhere, Category = "Sparks", meta = (ClampMin = "0.0"))
    float intensity{20.0f};

    UPROPERTY(EditAnywhere,
              Category = "Sparks",
              meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
    float spread_angle_degrees{90.0f};

    UPROPERTY(EditAnywhere, Category = "Sparks", meta = (ClampMin = "0.0", Units = "s"))
    float streak_time{0.025f};
};

struct SPACEGAMERENDERING_API FSparkBurst {
    FVector3f location{FVector3f::ZeroVector};
    FVector3f direction{FVector3f::UpVector};
    FLinearColor colour{FLinearColor::White};
    FSparkScalarRange speed;
    FSparkScalarRange lifetime;
    FSparkScalarRange size;
    float intensity{1.0f};
    float spread_angle_degrees{90.0f};
    float streak_time{0.0f};
    int32 count{0};
    uint32 seed{0};
};

USTRUCT(BlueprintType)
struct SPACEGAMERENDERING_API FSparkRendererSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Sparks", meta = (ClampMin = "1"))
    int32 capacity{100000};

    UPROPERTY(EditAnywhere, Category = "Sparks", meta = (ClampMin = "0.0", Units = "cm"))
    float maximum_draw_distance{200000.0f};

    UPROPERTY(EditAnywhere, Category = "Sparks")
    FVector3f acceleration{0.0f, 0.0f, -980.0f};

    UPROPERTY(EditAnywhere, Category = "Sparks", meta = (ClampMin = "0.0"))
    float minimum_thickness_pixels{0.75f};

    UPROPERTY(EditAnywhere, Category = "Sparks", meta = (ClampMin = "0.0"))
    float maximum_thickness_pixels{12.0f};

    UPROPERTY(EditAnywhere, Category = "Sparks", meta = (ClampMin = "0.0"))
    float maximum_length_pixels{128.0f};
};

struct alignas(16) SPACEGAMERENDERING_API FSparkParticleRecord {
    FVector4f initial_position_spawn_time{FVector4f::Zero()};
    FVector4f initial_velocity_lifetime{FVector4f::Zero()};
    FVector4f emissive_colour_size{FVector4f::Zero()};
    FVector4f streak_time_reserved{FVector4f::Zero()};
};

static_assert(sizeof(FSparkParticleRecord) == 64);

auto SPACEGAMERENDERING_API expand_spark_bursts(TConstArrayView<FSparkBurst> bursts,
                                                int32 capacity,
                                                float effect_time,
                                                TArray<FSparkParticleRecord>& output) -> int64;

UCLASS(ClassGroup = (Rendering))
class SPACEGAMERENDERING_API USparkRendererComponent final : public UPrimitiveComponent {
    GENERATED_BODY()
  public:
    USparkRendererComponent();

    void initialise(FSparkRendererSettings const& settings);
    void clear_sparks();
    auto submit_particles(TConstArrayView<FSparkParticleRecord> particles, float effect_time)
        -> int32;

    auto get_effect_time() const noexcept -> float { return effect_time_; }
    auto get_capacity() const noexcept -> int32 { return particle_data_.Num(); }

    FPrimitiveSceneProxy* CreateSceneProxy() override;
    FBoxSphereBounds CalcBounds(FTransform const& local_to_world) const override;
    void SendRenderDynamicData_Concurrent() override;
    void GetUsedMaterials(TArray<UMaterialInterface*>& out_materials,
                          bool get_debug_materials = false) const override;
  private:
    struct FUploadRange {
        int32 first_index{0};
        TArray<FSparkParticleRecord> particles;
    };

    UPROPERTY()
    TObjectPtr<UMaterialInterface> material_;

    FSparkRendererSettings settings_;
    TArray<FSparkParticleRecord> particle_data_;
    TArray<FUploadRange> pending_uploads_;
    int32 allocation_cursor_{0};
    float effect_time_{0.0f};
    float latest_expiry_time_{0.0f};
};

class SPACEGAMERENDERING_API FSparkEffects {
  public:
    explicit FSparkEffects(USparkRendererComponent& renderer);

    void queue_burst(FSparkBurst const& burst);
    void commit(float dt);
    void clear();
  private:
    USparkRendererComponent* renderer_{nullptr};
    TArray<FSparkBurst> queued_bursts_;
    TArray<FSparkParticleRecord> expanded_particles_;
    float effect_time_{0.0f};
    int64 dropped_bursts_{0};
};
