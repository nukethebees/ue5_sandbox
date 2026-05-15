#pragma once

#include "Sandbox/logging/ActorLoggingConfig.h"
#include "Sandbox/misc/learning/TestMaterialConfig.h"
#include "Sandbox/players/VisionConfig.h"
#include "Sandbox/utilities/DrawDebugConfig.h"
#include "TestFlyBase.h"

#include "CoreMinimal.h"

#include "TestFlySearchChase.generated.h"

UENUM()
enum class ETestFlySearchChaseState : uint8 {
    searching,
    chasing,
};

class ATestVolume;

UCLASS()
class ATestFlySearchChase : public ATestFlyBase {
  public:
    GENERATED_BODY()

    ATestFlySearchChase();

    void Tick(float dt) override;
  protected:
    void BeginPlay() override;

    // Movement
    void move_to_location(float dt, FVector const& location);
    auto within_radius(FVector const& point, float const r) const -> bool;
    auto at_destination() const -> bool;

    // State
    void set_state(ETestFlySearchChaseState new_state);

    // Search
    void handle_search(float dt);
    void set_new_search_destination();
    void reset_search_destination();
    auto scan_for_target() -> bool;

    // Chase
    void handle_chase(float dt);

    // Debugging
    void draw_debug_shapes();

    // Visuals
    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestMaterialConfig search_material_config;
    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestMaterialConfig chase_material_config;

    // Movement
    UPROPERTY(EditAnywhere, Category = "Fly")
    float speed{1000.f};
    UPROPERTY(EditAnywhere, Category = "Fly")
    float turn_speed_deg_per_s{60.f};
    UPROPERTY(EditAnywhere, Category = "Fly")
    float acceptance_radius{500.f};

    // Vision
    UPROPERTY(EditAnywhere, Category = "Fly")
    FVisionConfig vision{};

    // State
    UPROPERTY(EditAnywhere, Category = "Fly")
    ETestFlySearchChaseState state{ETestFlySearchChaseState::searching};

    // Search
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    FVector search_destination;
    UPROPERTY(EditAnywhere, Category = "Fly")
    TObjectPtr<ATestVolume> search_volume{nullptr};
    UPROPERTY(EditAnywhere, Category = "Fly")
    float min_distance_to_new_point{5000.f};

    // Chase
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    TWeakObjectPtr<AActor> chase_target{nullptr};

    // Debugging
    UPROPERTY(EditAnywhere, Category = "Fly")
    bool show_debug_shapes{false};
    UPROPERTY(EditAnywhere, Category = "Fly")
    FDrawDebugConfig draw_config;
    UPROPERTY(EditAnywhere, Category = "Ship")
    FActorLoggingConfig log_config{1.f};
    UPROPERTY(EditAnywhere, Category = "Ship")
    float destination_sphere_radius{100.f};
};
