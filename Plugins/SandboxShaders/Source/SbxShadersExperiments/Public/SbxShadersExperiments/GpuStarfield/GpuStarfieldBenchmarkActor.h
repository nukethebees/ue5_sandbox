#pragma once

#include "CoreMinimal.h"
#include "SandboxShaders/GpuStarfield/GpuStarfieldActor.h"

#include "GpuStarfieldBenchmarkActor.generated.h"

class ACameraActor;

UCLASS(Blueprintable)
class SBXSHADERSEXPERIMENTS_API AGpuStarfieldBenchmarkActor final : public AGpuStarfieldActor {
    GENERATED_BODY()
  public:
    AGpuStarfieldBenchmarkActor();

    void BeginPlay() override;
    void EndPlay(EEndPlayReason::Type end_play_reason) override;
    void Tick(float delta_seconds) override;
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
