// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <utility>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"

#include "PushZoneActor.generated.h"

class UPrimitiveComponent;
class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EPushForceMode : uint8 {
    ContinuousForce UMETA(DisplayName = "Continuous Push"),
    InstantImpulse UMETA(DisplayName = "Instant Burst")
};

UCLASS()
class SANDBOX_API APushZoneActor
    : public AActor
    , public ml::LogMsgMixin<"PushZone"> {
    GENERATED_BODY()
  public:
    APushZoneActor();
  protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(FTransform const& Transform) override;
  public:
    virtual void Tick(float DeltaTime) override;

    // Configurable properties
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Push Zone")
    float max_force_strength{1000.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Push Zone")
    float min_distance_threshold{1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Push Zone")
    float custom_tick_interval{0.05f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Push Zone")
    EPushForceMode force_mode{EPushForceMode::ContinuousForce};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Push Zone")
    float force_multiplier{5000000.0};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Push Zone")
    float impulse_multiplier{500.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool show_debug_visualization{false};

    // Overlap event handlers
    UFUNCTION()
    void on_overlap_begin(UPrimitiveComponent* overlapped_component,
                          AActor* other_actor,
                          UPrimitiveComponent* other_component,
                          int32 other_body_index,
                          bool from_sweep,
                          FHitResult const& sweep_result);

    UFUNCTION()
    void on_overlap_end(UPrimitiveComponent* overlapped_component,
                        AActor* other_actor,
                        UPrimitiveComponent* other_component,
                        int32 other_body_index);
  protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* collision_box;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* visual_mesh_outer;
  private:
    // Visual mesh components
    UStaticMeshComponent* visual_mesh_inner{nullptr};

    // Visual properties
    UPROPERTY(EditAnywhere, Category = "Visual")
    float inner_shell_scale_ratio{0.98f};

    UPROPERTY(EditAnywhere, Category = "Visual")
    float inner_opacity_multiplier{0.8f};

    UPROPERTY(EditAnywhere, Category = "Visual")
    FLinearColor inner_color_tint{1.0f, 0.9f, 0.9f, 1.0f};

    // Actor tracking
    TArray<TWeakObjectPtr<AActor>> overlapping_actors;

    // Custom tick management
    FTimerHandle tick_timer_handle;
    bool is_zone_occupied{false};

    // Force calculation and application
    void apply_push_forces();
    FVector calculate_push_force(AActor* target_actor) const;
    void apply_force_to_actor(AActor* target_actor, FVector const& force) const;

    // Actor management
    void start_custom_ticking();
    void stop_custom_ticking();
    void cleanup_invalid_actors();

    // Debug visualization
    void draw_debug_info() const;
};
