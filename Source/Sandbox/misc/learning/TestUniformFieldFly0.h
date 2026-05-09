#pragma once

#include "Sandbox/core/Cooldown.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestUniformFieldFly0.generated.h"

class UStaticMeshComponent;

class ATestUniformField;

UCLASS()
class ATestUniformFieldFly0 : public AActor {
    GENERATED_BODY()
  public:
    ATestUniformFieldFly0();

    void Tick(float dt) override;
  protected:
    void BeginPlay() override;
    void OnConstruction(FTransform const& transform) override;
    void EndPlay(EEndPlayReason::Type const reason) override;

    void set_new_destination();
    void move_to_destination(float dt);
    void display_destination();
    bool at_target() const;
    void handle_oob();
    bool assert_field_exists();

    bool can_log() const;

    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};
    UPROPERTY(EditInstanceOnly, Category = "Ship")
    TObjectPtr<ATestUniformField> field{nullptr};

    // Navigation
    UPROPERTY(EditAnywhere, Category = "Ship")
    FVector destination{FVector::ZeroVector};
    UPROPERTY(EditAnywhere, Category = "Ship")
    bool show_destination{true};
    UPROPERTY(EditAnywhere, Category = "Ship")
    float acceptance_radius{1000.f};
    UPROPERTY(EditAnywhere, Category = "Ship")
    float min_dist{5000.f};
    UPROPERTY(EditAnywhere, Category = "Ship")
    float max_dist{10000.f};
    UPROPERTY(EditAnywhere, Category = "Ship")
    FCooldown arrival_cooldown{0.5f};
    // Movement
    UPROPERTY(EditAnywhere, Category = "Ship")
    float speed{1000.f};
    // Learning
    UPROPERTY(EditAnywhere, Category = "Ship")
    bool enable_log_prints{false};
    UPROPERTY(EditAnywhere, Category = "Ship")
    FCooldown log_cooldown{2.f};
};
