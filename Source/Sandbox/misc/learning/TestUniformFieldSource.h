#pragma once

#include "Sandbox/core/Cooldown.h"

#include "TestUniformFieldPointSourceData.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestUniformFieldSource.generated.h"

class UStaticMeshComponent;
class ATestUniformField;

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

    auto can_log() const;

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

    UPROPERTY(VisibleAnywhere, Category = "Field")
    bool enable_log_prints{false};
    UPROPERTY(VisibleAnywhere, Category = "Grid")
    FCooldown log_cooldown{1.f};
};
