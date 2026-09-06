#pragma once

#include <Components/PrimitiveComponent.h>
#include <CoreMinimal.h>
#include <SpaceGameRendering/SparkParticleRecord.h>
#include <SpaceGameRendering/SparkRendererSettings.h>

#include "SparkRendererComponent.generated.h"

struct FSparkStagingState;
struct FSparkUploadBuffer;

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
    UPROPERTY()
    TObjectPtr<UMaterialInterface> material_;

    FSparkRendererSettings settings_;
    TArray<FSparkParticleRecord> particle_data_;
    TSharedPtr<FSparkStagingState, ESPMode::ThreadSafe> staging_state_;
    FSparkUploadBuffer* pending_upload_buffer_{nullptr};
    int32 allocation_cursor_{0};
    float effect_time_{0.0f};
    float latest_expiry_time_{0.0f};
};
