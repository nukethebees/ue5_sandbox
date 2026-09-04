#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SbxShadersExperiments/GpuStarfield/GpuStarfieldComponent.h"

#include "GpuStarfieldExperimentActor.generated.h"

class ACameraActor;

UCLASS(Blueprintable)
class SBXSHADERSEXPERIMENTS_API AGpuStarfieldExperimentActor final : public AActor {
    GENERATED_BODY()
  public:
    AGpuStarfieldExperimentActor();

    void BeginPlay() override;
    void EndPlay(EEndPlayReason::Type end_play_reason) override;
    void Tick(float delta_seconds) override;
    void OnConstruction(FTransform const& transform) override;
    void PostRegisterAllComponents() override;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "GPU Starfield")
    void apply_settings();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPU Starfield")
    FGpuStarfieldSettings settings;
  private:
    struct FBenchmarkPhase {
        int32 star_count{0};
        int32 repeat_index{0};
        bool enabled{false};
        bool camera_motion{false};
    };

    void start_benchmark();
    void begin_benchmark_phase();
    void begin_csv_capture();
    void finish_benchmark_phase(FString const& filename);
    void update_benchmark_camera();

    UPROPERTY(VisibleAnywhere, Category = "GPU Starfield")
    TObjectPtr<UGpuStarfieldComponent> starfield_component_;

    TArray<FBenchmarkPhase> benchmark_phases_;
    FString benchmark_output_directory_;
    FDelegateHandle csv_finished_delegate_;
    TWeakObjectPtr<ACameraActor> benchmark_camera_;
    FVector benchmark_camera_origin_{FVector::ZeroVector};
    double benchmark_max_camera_distance_{0.0};
    int32 benchmark_phase_index_{0};
    int32 benchmark_warmup_frames_{0};
    int32 benchmark_capture_frames_{0};
    int32 benchmark_frames_remaining_{0};
    int32 benchmark_motion_frame_{0};
    bool benchmark_active_{false};
    bool benchmark_capture_pending_{false};
};
