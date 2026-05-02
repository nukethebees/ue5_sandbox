#pragma once

#include "TestUniformFieldSource.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestUniformField.generated.h"

USTRUCT()
struct FTestUniformFieldCell {
    GENERATED_BODY()

    UPROPERTY()
    FVector potential{FVector::ZeroVector};
};

UCLASS()
class ATestUniformField : public AActor {
    GENERATED_BODY()
  public:
    ATestUniformField();

    void Tick(float dt) override;

    void add_source(FTestUniformFieldPointSource const& source);
  protected:
    void BeginPlay() override;
    void OnConstruction(FTransform const& transform) override;

    void update_cells();

    UPROPERTY(EditAnywhere, Category = "Grid")
    FIntVector grid_dimensions{1, 1, 1};

    UPROPERTY(EditAnywhere, Category = "Grid")
    TArray<FTestUniformFieldCell> cells{};

    UPROPERTY(VisibleAnywhere, Category = "Grid")
    TArray<FTestUniformFieldPointSource> point_sources{};
};
