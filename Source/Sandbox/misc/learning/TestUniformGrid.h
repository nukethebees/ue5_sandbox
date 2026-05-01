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
struct FTestUniformGridBoxPreviewSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Grid")
    bool visible{true};
    UPROPERTY(EditAnywhere, Category = "Grid")
    FLinearColor colour{FLinearColor::Blue};
    UPROPERTY(EditAnywhere, Category = "Grid")
    float opacity_edge_start{0.3f};
};

USTRUCT()
struct FTestUniformGridPointVisuals {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Grid")
    bool visible{false};
    UPROPERTY(EditAnywhere, Category = "Grid")
    FColor colour{FColor::Green};
    UPROPERTY(EditAnywhere, Category = "Grid")
    float size{12.f};
};

USTRUCT()
struct FScale3D {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Grid")
    bool use_uniform_scale{true};
    UPROPERTY(EditAnywhere, Category = "Grid", meta = (EditCondition = "!use_uniform_scale"))
    FVector non_uniform_scale{FVector::OneVector};
    UPROPERTY(EditAnywhere, Category = "Grid", meta = (EditCondition = "use_uniform_scale"))
    float uniform_scale{1.f};

    auto get() const -> FVector;
};

USTRUCT()
struct FTestUniformGridCellPreviewSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Grid")
    bool visible{true};

    UPROPERTY(EditAnywhere, Category = "Grid")
    FScale3D scale{};

    auto get_scale() const -> FVector { return scale.get(); }
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
    auto get_cell_transform(FVector const& position, FVector const& mesh_extent, FVector const& scale_factor) const
        -> FTransform;
    auto get_num_cells() const {
        return grid_cell_counts.X * grid_cell_counts.Y * grid_cell_counts.Z;
    }
  protected:
    void BeginPlay() override;
    void OnConstruction(FTransform const& transform) override;
    // Grid
    void update_grid();
    void update_grid_coordinates();
    // Box
    void configure_box();
    void configure_preview_mesh();
    void create_material_instance();
    // Cells
    void configure_cell_ism(UHierarchicalInstancedStaticMeshComponent& hism);
    void configure_cell_ism();
    void draw_cell_meshes();
    void draw_grid_debug_points();

#if WITH_EDITOR
    void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

    void update_dimensions();
#endif

    UPROPERTY(EditAnywhere, Category = "Grid")
    TObjectPtr<UBoxComponent> volume_box{nullptr};
    UPROPERTY(EditAnywhere, Category = "Grid")
    TObjectPtr<UStaticMeshComponent> preview_mesh{nullptr};
    // Can't show this in the editor due to performance issues
    UPROPERTY(EditDefaultsOnly, Category = "Grid")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> cell_bounds_instances{nullptr};
    UPROPERTY(EditDefaultsOnly, Category = "Grid")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> cell_points_instances{nullptr};

    // Box size
    UPROPERTY(EditAnywhere, Category = "Grid")
    FVector box_extent{1000.f, 1000.f, 1000.f}; // Half size
    UPROPERTY(EditAnywhere, Category = "Grid")
    FVector cell_extent{100.f, 100.f, 100.f};
    UPROPERTY(VisibleAnywhere, Category = "Grid")
    FIntVector grid_cell_counts{};

#if WITH_EDITORONLY_DATA
    UPROPERTY(VisibleAnywhere, Category = "Grid|Editor", meta = (DisplayName = "Num cells"))
    int32 num_cells_{0};
    UPROPERTY(VisibleAnywhere, Category = "Grid|Editor", meta = (DisplayName = "Box dimensions"))
    FVector box_dimensions_{};
    UPROPERTY(VisibleAnywhere, Category = "Grid|Editor", meta = (DisplayName = "Cell dimensions"))
    FVector cell_dimensions_{};
#endif

    // Visualisation
    UPROPERTY(EditAnywhere, Category = "Grid")
    UMaterialInterface* preview_material_parent{nullptr};
    UPROPERTY(VisibleAnywhere, Category = "Grid")
    UMaterialInstanceDynamic* preview_material_instance{nullptr};
    UPROPERTY(EditAnywhere, Category = "Grid")
    FTestUniformGridBoxPreviewSettings box_preview_settings{};
    UPROPERTY(EditAnywhere, Category = "Grid")
    FTestUniformGridCellPreviewSettings cell_bounds_preview_settings{};
    UPROPERTY(EditAnywhere, Category = "Grid")
    FTestUniformGridCellPreviewSettings cell_points_preview_settings{};
    UPROPERTY(EditAnywhere, Category = "Grid")
    FTestUniformGridPointVisuals point_visuals{};
};
