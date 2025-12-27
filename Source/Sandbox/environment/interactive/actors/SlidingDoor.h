#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "SlidingDoor.generated.h"

class UPrimitiveComponent;
class UCapsuleComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ESlidingDoorDirection : uint8 {
    Left UMETA(DisplayName = "Left"),
    Right UMETA(DisplayName = "Right"),
    Up UMETA(DisplayName = "Up"),
    Down UMETA(DisplayName = "Down")
};

UENUM(BlueprintType)
enum class ESlidingDoorState : uint8 { Closed, Opening, Open, Closing };

UCLASS()
class SANDBOX_API ASlidingDoor : public AActor {
    GENERATED_BODY()
  public:
    ASlidingDoor();
  protected:
    virtual void BeginPlay() override;
  public:
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* door_mesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
    UCapsuleComponent* trigger_volume;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door Settings")
    float open_speed{200.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door Settings")
    ESlidingDoorDirection direction{ESlidingDoorDirection::Right};

    UFUNCTION()
    void on_trigger_begin_overlap(UPrimitiveComponent* overlapped_component,
                                  AActor* other_actor,
                                  UPrimitiveComponent* other_component,
                                  int32 other_body_index,
                                  bool from_sweep,
                                  FHitResult const& sweep_result);

    UFUNCTION()
    void on_trigger_end_overlap(UPrimitiveComponent* overlapped_component,
                                AActor* other_actor,
                                UPrimitiveComponent* other_component,
                                int32 other_body_index);
  private:
    int32 actor_count{0};
    float current_progress{0.0f};
    FVector closed_position{};
    ESlidingDoorState door_state{ESlidingDoorState::Closed};

    FVector calculate_target_offset() const;
    void update_door_state();
    void update_door_position(float DeltaTime);
};
