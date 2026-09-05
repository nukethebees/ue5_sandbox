#pragma once

#include "SandboxISMCInstanceData.h"

#include "GameFramework/Actor.h"

#include "SandboxISMCLabActor.generated.h"

class UStaticMesh;
class USandboxISMCComponent;

UENUM()
enum class ESandboxISMCLabDistribution : uint8 {
    Grid,
    Cloud,
};

UCLASS()
class SANDBOXISMCLAB_API ASandboxISMCLabActor final : public AActor {
    GENERATED_BODY()
  public:
    ASandboxISMCLabActor();

    virtual void OnConstruction(FTransform const& transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float delta_seconds) override;

    UFUNCTION(CallInEditor, Category = "Sandbox ISMC Lab")
    void regenerate_instances();

    UFUNCTION(CallInEditor, Category = "Sandbox ISMC Lab")
    void clear_instances();
  private:
    void submit_instances();

    UPROPERTY(VisibleAnywhere, Category = "Sandbox ISMC Lab")
    TObjectPtr<USandboxISMCComponent> instances_;

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Lab")
    TObjectPtr<UStaticMesh> static_mesh_;

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Lab")
    ESandboxISMCLabDistribution distribution_{ESandboxISMCLabDistribution::Grid};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Lab|Grid", meta = (ClampMin = "1"))
    FIntVector grid_dimensions_{200, 200, 1};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Lab|Grid")
    FVector grid_spacing_{150.0, 150.0, 150.0};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Lab|Cloud", meta = (ClampMin = "1"))
    int32 cloud_instance_count_{40000};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Lab|Cloud")
    FVector cloud_extent_{10000.0, 10000.0, 1000.0};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Lab|Cloud")
    int32 random_seed_{1337};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Lab|Runtime")
    bool regenerate_on_begin_play_{true};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Lab|Editor")
    bool regenerate_on_construction_{false};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Lab|Runtime")
    bool animate_{true};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Lab|Runtime", meta = (ClampMin = "0"))
    int32 animated_instance_count_{1024};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Lab|Runtime")
    float rotation_speed_degrees_{45.0f};

    UPROPERTY(EditAnywhere, Category = "Sandbox ISMC Lab|Instrumentation")
    bool log_metrics_{true};

    UPROPERTY(EditAnywhere,
              Category = "Sandbox ISMC Lab|Instrumentation",
              meta = (ClampMin = "0.1"))
    float log_interval_seconds_{1.0f};

    ml::sandbox_ismc::InstanceData instance_data_;
    double next_log_time_seconds_{0.0};
};
