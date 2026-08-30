#pragma once

#include "GameFramework/Actor.h"

#include "SandboxISMCBenchmarkActor.generated.h"

class UCameraComponent;
class UInstancedStaticMeshComponent;
class USandboxISMCComponent;
class USceneComponent;
class UStaticMesh;

UCLASS()
class SANDBOXISMCLAB_API ASandboxISMCBenchmarkActor final : public AActor {
    GENERATED_BODY()
  public:
    ASandboxISMCBenchmarkActor();

    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type end_play_reason) override;
    virtual void Tick(float delta_seconds) override;
  private:
    struct FUpdateTiming {
        double total_ms{0.0};
        double prepare_ms{0.0};
        double pack_bounds_ms{-1.0};
        double api_ms{0.0};
    };

    struct FRendererSamples {
        TArray<double> total_update_ms;
        TArray<double> prepare_ms;
        TArray<double> pack_bounds_ms;
        TArray<double> api_ms;
    };

    void configure_components();
    bool create_instances();
    FUpdateTiming update_custom(float vertical_offset, float angle_radians);
    FUpdateTiming update_engine_ismc(float vertical_offset, float angle_radians);
    void record_samples(FRendererSamples& samples, FUpdateTiming const& timing);
    void start_insights_trace();
    void stop_insights_trace();
    void disable_frame_rate_limits();
    void restore_frame_rate_limits();
    void save_report() const;

    UPROPERTY(VisibleAnywhere, Category = "Sandbox ISMC Benchmark")
    TObjectPtr<USceneComponent> root_;

    UPROPERTY(VisibleAnywhere, Category = "Sandbox ISMC Benchmark")
    TObjectPtr<UCameraComponent> camera_;

    UPROPERTY(VisibleAnywhere, Category = "Sandbox ISMC Benchmark")
    TObjectPtr<USandboxISMCComponent> custom_ismc_;

    UPROPERTY(VisibleAnywhere, Category = "Sandbox ISMC Benchmark")
    TObjectPtr<UInstancedStaticMeshComponent> engine_ismc_;

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark")
    TObjectPtr<UStaticMesh> static_mesh_;

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark", meta = (ClampMin = "1"))
    int32 instance_count_{40000};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark")
    float grid_spacing_{150.0f};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark")
    float grid_gap_{2000.0f};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark|Movement")
    float vertical_movement_amplitude_{50.0f};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark|Movement")
    float movement_frequency_hz_{0.1f};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark|Movement")
    float rotation_speed_degrees_{15.0f};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark|Output")
    bool capture_insights_trace_{true};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark|Output")
    bool save_csv_{true};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark|Timing")
    bool disable_frame_rate_limits_{true};

    TArray<FVector3f> base_positions_;
    TArray<FTransform> engine_update_transforms_;
    TArray<double> frame_ms_;
    FRendererSamples custom_samples_;
    FRendererSamples engine_samples_;

    float animation_elapsed_seconds_{0.0f};
    bool running_{false};
    bool owns_insights_trace_{false};
    bool frame_rate_limits_disabled_{false};
    int32 previous_vsync_{0};
    int32 previous_editor_vsync_{0};
    float previous_max_fps_{0.0f};
    double custom_creation_ms_{0.0};
    double engine_creation_ms_{0.0};
    FString output_base_name_;
};
