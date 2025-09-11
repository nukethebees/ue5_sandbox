// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <utility>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "PushZoneActor.generated.h"

class UPrimitiveComponent;

namespace ml {
inline static constexpr wchar_t PushZoneActorLogTag[]{TEXT("PushZone")};
}

UCLASS()
class SANDBOX_API APushZoneActor
    : public AActor
    , public ml::LogMsgMixin<ml::PushZoneActorLogTag> {
    GENERATED_BODY()
  public:
    APushZoneActor();
  protected:
    virtual void BeginPlay() override;
  public:
    virtual void Tick(float DeltaTime) override;

    // Configurable properties
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Push Zone")
    float max_force_strength{1000.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Push Zone")
    float min_distance_threshold{10.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Push Zone")
    float custom_tick_interval{0.1f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Push Zone")
    bool use_impulse_force{false};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool show_debug_visualization{false};

    // Overlap event handlers
    UFUNCTION()
    void on_overlap_begin(UPrimitiveComponent* OverlappedComponent,
                          AActor* OtherActor,
                          UPrimitiveComponent* OtherComponent,
                          int32 OtherBodyIndex,
                          bool bFromSweep,
                          FHitResult const& SweepResult);

    UFUNCTION()
    void on_overlap_end(UPrimitiveComponent* OverlappedComponent,
                        AActor* OtherActor,
                        UPrimitiveComponent* OtherComponent,
                        int32 OtherBodyIndex);
  protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* root_scene;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* collision_box;
  private:
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
