#pragma once

#include "GameFramework/Actor.h"
#include "Math/Box.h"
#include "SandboxISMCUpdateMetrics.h"

#include "SandboxISMCBenchmarkActor.generated.h"

class UCameraComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class USandboxISMCComponent;
class USceneComponent;
class UStaticMesh;

UENUM()
enum class ESandboxISMCBenchmarkMode : uint8 {
    Paired,
    CustomOnly,
    EngineISMCOnly,
};

UENUM()
enum class ESandboxISMCBenchmarkVisibility : uint8 {
    All,
    Half,
    None,
};

UENUM()
enum class ESandboxISMCBenchmarkCustomData : uint8 {
    None,
    StaticRgb,
    AnimatedRgb,
};

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
        double prepare_ms{-1.0};
        double build_ms{-1.0};
        double api_ms{0.0};
        double transform_upload_bytes{-1.0};
        double custom_data_upload_bytes{-1.0};
        double uploaded_bytes{-1.0};
    };

    struct FRendererSamples {
        TArray<double> total_update_ms;
        TArray<double> prepare_ms;
        TArray<double> build_ms;
        TArray<double> api_ms;
        TArray<double> transform_upload_bytes;
        TArray<double> custom_data_upload_bytes;
        TArray<double> uploaded_bytes;
        TArray<double> growing_update_ms;
        TArray<double> shrinking_update_ms;
        TArray<double> steady_update_ms;
    };

    void parse_command_line();
    void configure_components();
    bool create_instances();
    auto advance_churn() -> void;
    FUpdateTiming update_custom(float vertical_offset, float angle_radians, float colour_alpha);
    FUpdateTiming
        update_engine_ismc(float vertical_offset, float angle_radians, float colour_alpha);
    void record_samples(FRendererSamples& samples, FUpdateTiming const& timing);
    void finish_benchmark();
    void start_insights_trace();
    void stop_insights_trace();
    void disable_frame_rate_limits();
    void restore_frame_rate_limits();
    void save_report() const;
    bool runs_custom() const;
    bool runs_engine_ismc() const;
    int32 get_update_count() const;
    FString get_mode_name() const;
    FString get_visibility_name() const;
    FString get_bounds_name() const;
    FString get_custom_data_name() const;
    bool uses_custom_data() const;
    bool animates_custom_data() const;

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

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark|Rendering")
    TObjectPtr<UMaterialInterface> custom_data_material_;

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark", meta = (ClampMin = "1"))
    int32 instance_count_{40000};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark|Churn")
    bool churn_enabled_{false};

    // Instance Count is the maximum population in churn mode.
    UPROPERTY(EditAnywhere,
              Category = "Sandbox ISMC Benchmark|Churn",
              meta = (ClampMin = "0", EditCondition = "churn_enabled_"))
    int32 minimum_live_count_{1000};

    UPROPERTY(EditAnywhere,
              Category = "Sandbox ISMC Benchmark|Churn",
              meta = (ClampMin = "1", EditCondition = "churn_enabled_"))
    int32 churn_half_cycle_updates_{120};

    UPROPERTY(EditAnywhere,
              Category = "Sandbox ISMC Benchmark|Churn",
              meta = (ClampMin = "0", ClampMax = "100", EditCondition = "churn_enabled_"))
    float replacement_percentage_{5.0f};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark|Timing", meta = (ClampMin = "0"))
    int32 warmup_updates_{0};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark")
    ESandboxISMCBenchmarkMode mode_{ESandboxISMCBenchmarkMode::Paired};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark")
    ESandboxISMCBenchmarkVisibility visibility_{ESandboxISMCBenchmarkVisibility::All};

    UPROPERTY(EditAnywhere,
              Category = "Sandbox ISMC Benchmark|Movement",
              meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float update_percentage_{100.0f};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark|Rendering")
    bool cast_shadows_{false};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark|Rendering")
    bool use_supplied_bounds_{false};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark|Rendering")
    ESandboxISMCBenchmarkCustomData custom_data_{ESandboxISMCBenchmarkCustomData::None};

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

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Benchmark|Timing", meta = (ClampMin = "0.0"))
    float automatic_stop_seconds_{0.0f};

    TArray<FVector3f> base_positions_;
    TArray<FVector3f> base_colours_;
    TArray<FTransform> engine_update_transforms_;
    TArray<float> engine_custom_data_;
    TArray<int32> engine_removals_;
    TArray<FTransform> engine_additions_;
    int32 live_count_{0};
    int32 previous_live_count_{0};
    int32 retained_count_{0};
    int64 update_index_{0};
    double replacement_remainder_{0.0};
    FSandboxISMCUpdateMetrics previous_metrics_;
    TArray<double> live_counts_;
    TArray<double> removed_counts_;
    TArray<double> added_counts_;
    TArray<double> staging_capacity_changes_;
    TArray<double> gpu_buffer_allocations_;
    TArray<double> staging_waits_;
    TArray<double> staging_wait_ms_;
    FBox3f supplied_local_bounds_{ForceInit};
    TArray<double> frame_ms_;
    TArray<double> game_thread_ms_;
    TArray<double> render_thread_ms_;
    TArray<double> gpu_ms_;
    FRendererSamples custom_samples_;
    FRendererSamples engine_samples_;

    float animation_elapsed_seconds_{0.0f};
    bool running_{false};
    bool request_end_pie_on_completion_{false};
    bool owns_insights_trace_{false};
    bool frame_rate_limits_disabled_{false};
    int32 previous_vsync_{0};
    int32 previous_editor_vsync_{0};
    float previous_max_fps_{0.0f};
    double custom_creation_ms_{0.0};
    double engine_creation_ms_{0.0};
    FString output_base_name_;
};
