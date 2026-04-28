#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/Color.h"

#include "TestUniformGrid.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UHierarchicalInstancedStaticMeshComponent;

struct FPropertyChangedEvent;

USTRUCT()
struct FTestUniformGridPreviewMaterialSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Grid")
    FLinearColor colour{FLinearColor::Blue};
    UPROPERTY(EditAnywhere, Category = "Grid")
    float opacity_edge_start{0.3f};
};

USTRUCT()
struct FTestUniformGridPointVisuals {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Grid")
    FColor colour{FColor::Green};
    UPROPERTY(EditAnywhere, Category = "Grid")
    float size{12.f};
};

USTRUCT()
struct FTestUniformGridCell {
    GENERATED_BODY()
};

UCLASS()
class ATestUniformGrid : public AActor {
  public:
    GENERATED_BODY()

    ATestUniformGrid();

    void Tick(float dt) override;

    auto get_box_origin() const -> FVector;
    auto get_grid_origin() const -> FVector;
    auto get_box_dimensions() const -> FVector { return 2.0 * box_extent; }
    auto get_cell_dimensions() const -> FVector { return 2.0 * cell_extent; }
    static auto get_cell_position(FVector const& origin,
                                  FVector const& cell_size,
                                  int32 const x,
                                  int32 const y,
                                  int32 const z) -> FVector {
        FVector const offset{
            static_cast<double>(x) * cell_size.X,
            static_cast<double>(y) * cell_size.Y,
            static_cast<double>(z) * cell_size.Z,
        };
        auto const pos{origin + offset};
        return pos;
    }
  protected:
    void BeginPlay() override;
    void OnConstruction(FTransform const& transform) override;
    void create_material_instance();
    void update_grid();
    void update_grid_coordinates();
    void draw_grid_points();
    void configure_preview_mesh();

#if WITH_EDITOR
    void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

    void update_dimensions();
#endif

    UPROPERTY(EditAnywhere, Category = "Grid")
    UBoxComponent* volume_box{nullptr};
    UPROPERTY(EditAnywhere, Category = "Grid")
    UStaticMeshComponent* preview_mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Grid")
    UHierarchicalInstancedStaticMeshComponent* cell_instances{nullptr};

    // Box size
    UPROPERTY(EditAnywhere, Category = "Grid")
    FVector box_extent{1000.f, 1000.f, 1000.f}; // Half size
    UPROPERTY(EditAnywhere, Category = "Grid")
    FVector cell_extent{100.f, 100.f, 100.f};
    UPROPERTY(VisibleAnywhere, Category = "Grid")
    FIntVector grid_cell_counts{};

#if WITH_EDITORONLY_DATA
    UPROPERTY(VisibleAnywhere, Category = "Grid")
    FVector box_dimensions{};
    UPROPERTY(VisibleAnywhere, Category = "Grid")
    FVector cell_dimensions{};
#endif

    // Visualisation
    UPROPERTY(EditAnywhere, Category = "Grid")
    bool show_preview{true};
    UPROPERTY(EditAnywhere, Category = "Grid")
    bool draw_grid_debug_shapes{false};
    UPROPERTY(EditAnywhere, Category = "Grid")
    UMaterialInterface* preview_material_parent{nullptr};
    UPROPERTY(VisibleAnywhere, Category = "Grid")
    UMaterialInstanceDynamic* preview_material_instance{nullptr};
    UPROPERTY(EditAnywhere, Category = "Grid")
    FTestUniformGridPreviewMaterialSettings preview_material_settings{};

    UPROPERTY(EditAnywhere, Category = "Grid")
    FTestUniformGridPointVisuals point_visuals{};
};
