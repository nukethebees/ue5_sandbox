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
    void OnConstruction(FTransform const& transform) override;
    void BeginPlay() override;
    void EndPlay(EEndPlayReason::Type const reason) override;

    void update_sources();
    void broadcast_to_field();
    void broadcast_update_to_field();
    void on_field_pre_construction(ATestUniformField& field_ref);

    UPROPERTY(EditAnywhere, Category = "Field")
    TObjectPtr<UStaticMeshComponent> point_mesh{nullptr};
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Field")
    TObjectPtr<class ATestUniformField> field{};
    UPROPERTY(EditAnywhere, Category = "Field")
    TArray<FTestUniformFieldPointSourceData> sources{};
    UPROPERTY(EditAnywhere, Category = "Field")
    bool use_tick{false};
    UPROPERTY(EditAnywhere, Category = "Field")
    bool is_dynamic{false};
    UPROPERTY(VisibleAnywhere, Category = "Field")
    int32 source_id{0};

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
