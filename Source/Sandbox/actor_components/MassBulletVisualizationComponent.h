#pragma once

#include <optional>

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/InstancedStaticMeshComponent.h"

#include "MassBulletVisualizationComponent.generated.h"

class UStaticMesh;
class ABulletActor;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UMassBulletVisualizationComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UMassBulletVisualizationComponent();

    std::optional<int32> add_instance(FTransform const& transform);
    void update_instance(int32 instance_index, FTransform const& transform);
    void remove_instance(int32 instance_index);

    UInstancedStaticMeshComponent* get_ismc() const { return ismc; }
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mass Bullet")
    UInstancedStaticMeshComponent* ismc{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass Bullet")
    TSubclassOf<ABulletActor> bullet_class;

    TArray<int32> free_indices{};
};
