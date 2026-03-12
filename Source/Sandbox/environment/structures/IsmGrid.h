#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "IsmGrid.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

USTRUCT(BlueprintType)
struct FGridAxis {
    GENERATED_BODY()

    constexpr FGridAxis() = default;
    constexpr FGridAxis(int32 p, int32 n)
        : positive(p)
        , negative(n) {}

    static constexpr auto OneVector() -> FGridAxis { return {1, 0}; }

    UPROPERTY(EditAnywhere)
    int32 positive{0};
    UPROPERTY(EditAnywhere)
    int32 negative{0};

    auto num() const { return positive + negative; }
};

UENUM()
enum class ERotationMode : uint8 { parent, from_centre };

UCLASS()
class AIsmGrid : public AActor {
    GENERATED_BODY()
  public:
    AIsmGrid();

    auto num() const { return x_axis.num() * y_axis.num() * z_axis.num(); }
  protected:
    void OnConstruction(FTransform const& transform) override;

    UFUNCTION(CallInEditor, Category = "IsmGrid")
    void update_isms_button();
    UFUNCTION(CallInEditor, Category = "IsmGrid")
    void update_isms(bool warn_on_no_mesh);

    UPROPERTY(VisibleAnywhere, Category = "IsmGrid")
    UInstancedStaticMeshComponent* ismc{nullptr};
    UPROPERTY(EditAnywhere, Category = "IsmGrid")
    UStaticMesh* mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "IsmGrid")
    FGridAxis x_axis{FGridAxis::OneVector()};
    UPROPERTY(EditAnywhere, Category = "IsmGrid")
    FGridAxis y_axis{FGridAxis::OneVector()};
    UPROPERTY(EditAnywhere, Category = "IsmGrid")
    FGridAxis z_axis{FGridAxis::OneVector()};
    UPROPERTY(EditAnywhere, Category = "IsmGrid")
    FVector3d fixed_offset{FVector3d::ZeroVector};
    UPROPERTY(EditAnywhere, Category = "IsmGrid")
    FVector3d element_offset{FVector3d::ZeroVector};
    UPROPERTY(EditAnywhere, Category = "IsmGrid")
    bool offset_by_mesh_bounds{true};
    UPROPERTY(EditAnywhere, Category = "IsmGrid")
    ERotationMode rotation_mode{ERotationMode::parent};
#if WITH_EDITORONLY_DATA
    UPROPERTY(VisibleAnywhere, Category = "IsmGrid|Debug")
    FVector3d mesh_bounds_{FVector3d::ZeroVector};
    UPROPERTY(VisibleAnywhere, Category = "IsmGrid|Debug")
    FVector3d mesh_bounds_offset_{FVector3d::ZeroVector};
    UPROPERTY(VisibleAnywhere, Category = "IsmGrid|Debug")
    FVector3d full_element_offset_{FVector3d::ZeroVector};
#endif
};
