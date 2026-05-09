#pragma once

#include "Sandbox/core/Cooldown.h"
#include "Sandbox/utilities/FloatBounds.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestUniformFieldFly2.generated.h"

class UStaticMeshComponent;

class ATestUniformField;

// This version can track an enemy

UCLASS()
class ATestUniformFieldFly2 : public AActor {
    GENERATED_BODY()
  public:
    ATestUniformFieldFly2();

    void Tick(float dt) override;
  protected:
    // Life cycle
    void BeginPlay() override;
    void OnConstruction(FTransform const& transform) override;
    void EndPlay(EEndPlayReason::Type const reason) override;

    // Navigation
    void set_new_destination();
    void move_to_destination(float dt);
    void display_destination();
    bool at_target() const;
    bool is_oob() const;

    // Vision
    void display_vision_cone();

    // Field
    bool assert_field_exists();

    // Targets
    void try_find_target();

    // Logging
    bool can_log() const;

    // Visuals
    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};

    // Navigation
    UPROPERTY(EditAnywhere, Category = "Ship")
    FVector destination{FVector::ZeroVector};
    UPROPERTY(EditAnywhere, Category = "Ship")
    bool show_destination{true};
    UPROPERTY(EditAnywhere, Category = "Ship")
    float acceptance_radius{1000.f};
    UPROPERTY(EditAnywhere, Category = "Ship")
    FFloatBounds dist_bounds{1000.f, 5000.f};

    // Movement
    UPROPERTY(EditAnywhere, Category = "Ship")
    float speed{1000.f};

    // Vision
    UPROPERTY(EditAnywhere, Category = "Ship")
    float vision_radius{1000.f};
    UPROPERTY(EditAnywhere, Category = "Ship")
    float vision_angle_deg{40.f};
    UPROPERTY(EditAnywhere, Category = "Ship")
    bool show_vision_cone{true};

    // Field
    UPROPERTY(EditInstanceOnly, Category = "Ship")
    TObjectPtr<ATestUniformField> field{nullptr};

    // Target
    UPROPERTY(EditInstanceOnly, Category = "Ship")
    TArray<TSubclassOf<AActor>> target_classes;
    UPROPERTY(VisibleInstanceOnly, Category = "Ship")
    TWeakObjectPtr<AActor> target{nullptr};

    // Logging
    UPROPERTY(EditAnywhere, Category = "Ship")
    bool enable_log_prints{false};
    UPROPERTY(EditAnywhere, Category = "Ship")
    FCooldown log_cooldown{2.f};
};
