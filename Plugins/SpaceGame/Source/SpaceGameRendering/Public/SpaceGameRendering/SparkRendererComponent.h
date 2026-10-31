#pragma once

#include <ioj/presentation/sparks.h>
#include <SpaceGameRendering/SparkRendererSettings.h>

#include <Components/PrimitiveComponent.h>
#include <CoreMinimal.h>

#include "SparkRendererComponent.generated.h"

struct FSparkStagingState;
struct FSparkUploadBuffer;
class FSparkParticleBuffer;

UCLASS(ClassGroup = (Rendering))
class SPACEGAMERENDERING_API USparkRendererComponent final : public UPrimitiveComponent {
    GENERATED_BODY()
  public:
    USparkRendererComponent();
    ~USparkRendererComponent() override;

    void initialise(FSparkRendererSettings const& settings);
    void clear_sparks();
    auto submit_bursts(std::span<ioj::presentation::SparkBurst const> bursts, float effect_time)
        -> ioj::presentation::SparkSubmissionResult;
    auto submit_particles(std::span<ioj::presentation::SparkParticleRecord const> particles,
                          float effect_time) -> int32;

    auto get_effect_time() const noexcept -> float { return effect_time_; }
    auto get_capacity() const noexcept -> int32 { return storage_.capacity(); }
    auto get_last_submission_upload_bytes() const noexcept -> int32 {
        return last_submission_upload_bytes_;
    }

    FPrimitiveSceneProxy* CreateSceneProxy() override;
    FBoxSphereBounds CalcBounds(FTransform const& local_to_world) const override;
    void SendRenderDynamicData_Concurrent() override;
    void GetUsedMaterials(TArray<UMaterialInterface*>& out_materials,
                          bool get_debug_materials = false) const override;
  private:
    UPROPERTY()
    TObjectPtr<UMaterialInterface> material_;

    ioj::presentation::SparkRendererSettings settings_;
    ioj::presentation::SparkStorage storage_;
    TSharedPtr<FSparkStagingState, ESPMode::ThreadSafe> staging_state_;
    FSparkUploadBuffer* pending_upload_buffer_{nullptr};
    ioj::presentation::SparkGpuBatch pending_gpu_batches_;
    FSparkParticleBuffer* particle_buffer_{nullptr};
    int32 last_submission_upload_bytes_{0};
    float effect_time_{0.0f};
    bool clear_pending_{false};
};
