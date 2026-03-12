#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "IsmSpline.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class USplineComponent;

UENUM()
enum class ESpawnMode : uint8 { spawn_per_distance, spread_over_distance };

UCLASS()
class AIsmSpline : public AActor {
    GENERATED_BODY()
  public:
    AIsmSpline();
  protected:
    void OnConstruction(FTransform const& transform) override;

    UFUNCTION(CallInEditor, Category = "IsmSpline")
    void update_isms_button();
    UFUNCTION(CallInEditor, Category = "IsmSpline")
    void update_isms(bool warn_on_no_mesh);
    bool update_values(this auto& self, USplineComponent& s);

    UPROPERTY(VisibleAnywhere, Category = "IsmSpline")
    UInstancedStaticMeshComponent* ismc{nullptr};
    UPROPERTY(VisibleAnywhere, Category = "IsmSpline")
    USplineComponent* spline{nullptr};
    UPROPERTY(EditAnywhere, Category = "IsmSpline")
    UStaticMesh* mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "IsmSpline")
    float distance_per_element_user{100.f};
    UPROPERTY(EditAnywhere, Category = "IsmSpline")
    int32 n_elems_user{3};
    UPROPERTY(EditAnywhere, Category = "IsmSpline")
    ESpawnMode spawn_mode{ESpawnMode::spawn_per_distance};

    UPROPERTY(VisibleAnywhere, Category = "IsmSpline")
    float distance_per_element_{0.f};
    UPROPERTY(VisibleAnywhere, Category = "IsmSpline")
    int32 n_elems_{0};
};
