#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/Color.h"

#include "TestUniformGrid.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

USTRUCT()
struct FTestUniformGridPreviewMaterialSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Grid")
    FLinearColor colour{FLinearColor::Blue};
    UPROPERTY(EditAnywhere, Category = "Grid")
    float opacity_edge_start{0.3f};
};

UCLASS()
class ATestUniformGrid : public AActor {
  public:
    GENERATED_BODY()

    ATestUniformGrid();
  protected:
    void BeginPlay() override;
    void OnConstruction(FTransform const& transform) override;
    void create_material_instance();

    UPROPERTY(EditAnywhere, Category = "Grid")
    UBoxComponent* volume_box{nullptr};
    UPROPERTY(EditAnywhere, Category = "Grid")
    UStaticMeshComponent* preview_mesh{nullptr};

    // Box size
    UPROPERTY(EditAnywhere, Category = "Grid")
    FVector box_extent{1000.f, 1000.f, 1000.f};
    UPROPERTY(EditAnywhere, Category = "Grid")
    FIntVector grid_dimensions{3, 3, 3};

    // Visualisation
    UPROPERTY(EditAnywhere, Category = "Grid")
    bool show_preview{true};
    UPROPERTY(EditAnywhere, Category = "Grid")
    UMaterialInterface* preview_material_parent{nullptr};
    UPROPERTY(VisibleAnywhere, Category = "Grid")
    UMaterialInstanceDynamic* preview_material_instance{nullptr};
    UPROPERTY(EditAnywhere, Category = "Grid")
    FTestUniformGridPreviewMaterialSettings preview_material_settings{};
};
