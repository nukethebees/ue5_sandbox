#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include <limits>

#include "TestUniformFieldSource.generated.h"

class UStaticMeshComponent;
class ATestUniformField;

USTRUCT()
struct FTestUniformFieldPointSourceData {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Field")
    FVector coordinate{};
    UPROPERTY(EditAnywhere, Category = "Field")
    float strength{0.f}; // Positive is repulsive
    UPROPERTY(EditAnywhere, Category = "Field")
    float falloff{-2.f}; // pow(distance, falloff)
    UPROPERTY(EditAnywhere, Category = "Field")
    FRotator rotation{FRotator::ZeroRotator};
    // The field is only present within the two radii
    UPROPERTY(EditAnywhere, Category = "Field")
    float inner_radius{0};
    UPROPERTY(EditAnywhere, Category = "Field")
    float outer_radius{std::numeric_limits<float>::max()};
};

UCLASS()
class ATestUniformFieldPointSource : public AActor {
    GENERATED_BODY()
  public:
    ATestUniformFieldPointSource();

    void Tick(float dt) override;
  protected:
    void BeginPlay() override;
    void OnConstruction(FTransform const& transform) override;

    void update_source();
    void find_field();
    void broadcast_to_field();

    UPROPERTY(EditAnywhere, Category = "Field")
    TObjectPtr<UStaticMeshComponent> point_mesh{nullptr};

    UPROPERTY()
    TWeakObjectPtr<ATestUniformField> field{nullptr};

    UPROPERTY(EditAnywhere, Category = "Field")
    FTestUniformFieldPointSourceData source{};

#if WITH_EDITOR
    auto can_log() const { return dbg_log_timer >= dbg_log_cooldown; }
#endif

#if WITH_EDITORONLY_DATA
    UPROPERTY(VisibleAnywhere, Category = "Grid")
    float dbg_log_cooldown{1.f};
    UPROPERTY(VisibleAnywhere, Category = "Grid")
    float dbg_log_timer{0};
#endif
};
