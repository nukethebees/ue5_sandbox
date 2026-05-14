#pragma once

#include "Sandbox/logging/ActorLoggingConfig.h"
#include "Sandbox/utilities/DrawDebugConfig.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestFlyCircularArc.generated.h"

class UStaticMeshComponent;

UCLASS()
class ATestFlyCircularArc : public AActor {
    GENERATED_BODY()
  public:
    ATestFlyCircularArc();

    void Tick(float dt) override;
  protected:
    void OnConstruction(FTransform const& transform) override;
    void BeginPlay() override;

    bool on_same_z() const;

    void initial_setup(AActor const& tgt);
    void draw_shapes() const;
    void move_to_point(float dt);

    UPROPERTY(EditAnywhere, Category = "Fly")
    float tick_interval{2.0f};

    // Visuals
    UPROPERTY(EditAnywhere, Category = "Fly")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Fly")
    FDrawDebugConfig draw_config;

    // Movement
    UPROPERTY(EditAnywhere, Category = "Fly")
    float speed{100.f};

    // Target
    UPROPERTY(EditAnywhere, Category = "Fly")
    TWeakObjectPtr<AActor> target_actor{nullptr};

    UPROPERTY(EditAnywhere, Category = "Fly")
    float angle_deg{90.f};

    UPROPERTY(VisibleAnywhere, Category = "Fly")
    float radius{1000.f};

    UPROPERTY(VisibleAnywhere, Category = "Fly")
    FVector starting_point{};
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    FVector mid_point{};
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    FVector destination{};
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    FVector circle_centre{};
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    FVector centre_to_mid{};

    UPROPERTY(VisibleAnywhere, Category = "Fly")
    FVector plane_normal{};
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    FVector side_direction{};

    UPROPERTY(VisibleAnywhere, Category = "Fly")
    float distance_from_target{};
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    float distance_from_midpoint{};
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    float circumfrence{};

    // Live debugging
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    float cur_angle_between_deg{};
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    float move_angle_between_deg{};
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    float cur_frac_left{};

    // Logging
    UPROPERTY(EditAnywhere, Category = "Ship")
    FActorLoggingConfig log_config{1.f};
};
