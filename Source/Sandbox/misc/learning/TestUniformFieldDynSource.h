#pragma once

#include "Sandbox/core/Cooldown.h"

#include "TestUniformFieldPointSourceData.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestUniformFieldDynSource.generated.h"

class UStaticMeshComponent;
class ATestUniformField;

UCLASS()
class ATestUniformFieldDynPointSource : public AActor {
    GENERATED_BODY()
  public:
    ATestUniformFieldDynPointSource();

    void Tick(float dt) override;
  protected:
    void BeginPlay() override;
    void EndPlay(EEndPlayReason::Type const reason) override;

    bool should_update_sources() const;
    void update_sources();
    void broadcast_to_field();
    void broadcast_update_to_field();

    void on_field_pre_construction(ATestUniformField& field_ref);

    void reset_destination();
    void update_destination();
    bool at_destination();
    void move_to_destination(float dt);

    bool can_log() const;

    UPROPERTY(EditAnywhere, Category = "Source")
    TObjectPtr<UStaticMeshComponent> point_mesh{nullptr};
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Source")
    TObjectPtr<class ATestUniformField> field{};

    UPROPERTY(EditAnywhere, Category = "Source")
    TArray<FTestUniformFieldPointSourceData> sources{};
    UPROPERTY(VisibleAnywhere, Category = "Source")
    int32 source_id{0};

    UPROPERTY(VisibleAnywhere, Category = "Source")
    FVector destination{FVector::ZeroVector};
    UPROPERTY(VisibleAnywhere, Category = "Source")
    FVector last_update_pos{FVector::ZeroVector};
    UPROPERTY(EditAnywhere, Category = "Source")
    float acceptance_radius{500.f};
    UPROPERTY(EditAnywhere, Category = "Source")
    float update_threshold{500.f};
    UPROPERTY(EditAnywhere, Category = "Source")
    float speed{3000.f};

    UPROPERTY(EditAnywhere, Category = "Source")
    bool enable_log_prints{false};
    UPROPERTY(VisibleAnywhere, Category = "Source")
    FCooldown log_cooldown{1.f};
};
