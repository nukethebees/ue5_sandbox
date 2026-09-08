#pragma once

#include "SbxMeshGenLab/MeshAssembly.h"
#include "SbxMeshGenLab/MeshAssemblyRecipe.h"
#include "ScopedTransaction.h"
#include "Tools/LegacyEdModeWidgetHelpers.h"

#include "SbxMeshGenLabEditorMode.generated.h"

class FEditorViewportClient;
class FCanvas;
class FPrimitiveDrawInterface;
class FSceneView;
class FViewport;
class HHitProxy;
class UInstancedStaticMeshComponent;
class USbxMeshAssemblyRecipe;
class USbxMeshGenLabSettings;
struct FViewportClick;

struct FSbxMeshPreviewBucket {
    FString mesh_key;
    TWeakObjectPtr<UInstancedStaticMeshComponent> component;
    TArray<FGuid> instance_part_ids;
};

struct FSbxMeshPreviewInstanceLocation {
    int32 bucket_index{INDEX_NONE};
    int32 instance_index{INDEX_NONE};
};

DECLARE_MULTICAST_DELEGATE(FOnSbxMeshSessionUndo);

UCLASS(Transient)
class SBXMESHGENLAB_API USbxMeshAssemblySessionState final : public UObject {
    GENERATED_BODY()
  public:
    void PostEditUndo() override;
    auto on_undo() -> FOnSbxMeshSessionUndo&;

    UPROPERTY(Transient)
    TArray<FSbxMeshAssemblyRecipePart> parts;

    UPROPERTY(Transient)
    TArray<FSbxMeshAssemblyRecipeGroup> groups;
  private:
    FOnSbxMeshSessionUndo undo_;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSbxMeshSessionChanged, bool);

UCLASS(Transient)
class SBXMESHGENLAB_API USbxMeshGenLabEditorMode final
    : public UBaseLegacyWidgetEdMode
    , public ILegacyEdModeSelectInterface {
    GENERATED_BODY()
  public:
    static FEditorModeID const mode_id;

    USbxMeshGenLabEditorMode();

    void Enter() override;
    void Exit() override;
    void CreateToolkit() override;
    void Render(FSceneView const* view,
                FViewport* viewport,
                FPrimitiveDrawInterface* primitive_draw_interface) override;
    void DrawHUD(FEditorViewportClient* viewport_client,
                 FViewport* viewport,
                 FSceneView const* view,
                 FCanvas* canvas) override;

    auto UsesTransformWidget() const -> bool override;
    auto ShouldDrawWidget() const -> bool override;
    auto GetWidgetLocation() const -> FVector override;
    auto InputDelta(FEditorViewportClient* viewport_client,
                    FViewport* viewport,
                    FVector& drag,
                    FRotator& rotation,
                    FVector& scale) -> bool override;
    auto StartTracking(FEditorViewportClient* viewport_client, FViewport* viewport)
        -> bool override;
    auto EndTracking(FEditorViewportClient* viewport_client, FViewport* viewport) -> bool override;
    auto HandleClick(FEditorViewportClient* viewport_client,
                     HHitProxy* hit_proxy,
                     FViewportClick const& click) -> bool override;
    auto BoxSelect(FBox& box, bool select = true) -> bool override;
    auto FrustumSelect(FConvexVolume const& frustum,
                       FEditorViewportClient* viewport_client,
                       bool select = true) -> bool override;
    void SelectNone() override;
    auto IsSelectionAllowed(AActor* actor, bool selecting) const -> bool override;
    void ActorSelectionChangeNotify() override;
    auto HasCustomViewportFocus() const -> bool override;
    auto ComputeCustomViewportFocus() const -> FBox override;

    [[nodiscard]] auto get_settings() const -> USbxMeshGenLabSettings*;
    [[nodiscard]] auto get_parts() const -> TArray<FSbxMeshAssemblyPart> const&;
    [[nodiscard]] auto get_recipe_parts() const -> TArray<FSbxMeshAssemblyRecipePart> const&;
    [[nodiscard]] auto get_selected_part_index() const -> int32;
    [[nodiscard]] auto get_selected_part_indices() const -> TArray<int32> const&;
    [[nodiscard]] auto get_groups() const -> TArray<FSbxMeshAssemblyRecipeGroup> const&;
    [[nodiscard]] auto get_selected_group_index() const -> int32;
    [[nodiscard]] auto can_remove_selected_parts() const -> bool;
    [[nodiscard]] auto can_create_group() const -> bool;
    [[nodiscard]] auto can_ungroup() const -> bool;
    [[nodiscard]] auto can_set_snap_target() const -> bool;
    [[nodiscard]] auto can_snap_selected_group() const -> bool;
    [[nodiscard]] auto get_status() const -> FText const&;
    [[nodiscard]] auto get_snap_target_text() const -> FText;
    [[nodiscard]] auto get_recipe_document_text() const -> FText;
    [[nodiscard]] auto has_current_recipe() const -> bool;
    auto on_session_changed() -> FOnSbxMeshSessionChanged&;

    void select_part(int32 part_index);
    void select_parts(TArray<int32> const& part_indices, int32 primary_part_index);
    void select_group(int32 group_index);
    void select_node(FGuid id);
    void select_nodes(TArray<FGuid> const& ids, FGuid primary_id);
    void select_all_parts();
    void selection_settings_changed();
    void add_part();
    void duplicate_part();
    void remove_part();
    void create_group();
    void ungroup();
    auto rename_group(FGuid id, FName name) -> bool;
    [[nodiscard]] auto can_reparent_nodes(TArray<FGuid> const& ids, FGuid parent_id) const -> bool;
    auto reparent_node(FGuid id, FGuid parent_id) -> bool;
    auto reparent_nodes(TArray<FGuid> const& ids, FGuid parent_id) -> bool;
    void set_snap_target();
    void align_connectors();
    void snap_and_parent();
    void new_assembly();
    void save_recipe();
    void save_recipe_as();
    void load_recipe();
    void export_recipe_json();
    void import_recipe_json();
    void apply_settings();
    void save_generated_mesh();
  private:
    void initialize_session();
    auto ensure_preview_actor() -> bool;
    auto find_or_create_preview_bucket(FSbxMeshGenerationRequest const& request) -> int32;
    void add_preview_instance(int32 part_index);
    void remove_preview_instance(FGuid part_id);
    void destroy_preview();
    void refresh_preview_instance(int32 part_index, bool rebuild_mesh);
    void select_preview_instances();
    void rebuild_part_index_map();
    void rebuild_resolved_parts(bool rebuild_geometry = false);
    void duplicate_selected_group();
    void apply_connector_snap(bool parent_to_target);
    void restore_session_after_undo();
    [[nodiscard]] auto get_group_world_transform(int32 group_index) const -> FTransform;
    [[nodiscard]] auto get_node_world_transform(FGuid id) const -> FTransform;
    [[nodiscard]] auto get_node_parent_id(FGuid id) const -> FGuid;
    [[nodiscard]] auto is_node_descendant(FGuid id, FGuid ancestor_id) const -> bool;
    [[nodiscard]] auto get_connector_world_transform(int32 group_index, int32 connector_index) const
        -> FTransform;
    [[nodiscard]] auto get_parent_world_transform(FGuid parent_id) const -> FTransform;
    void set_part_world_transform(int32 part_index, FTransform const& transform);
    void set_group_world_transform(int32 group_index, FTransform const& transform);
    [[nodiscard]] auto get_descendant_part_indices(FGuid group_id) const -> TArray<int32>;
    [[nodiscard]] auto get_descendant_group_indices(FGuid group_id) const -> TArray<int32>;
    void apply_settings(bool mark_dirty);
    void save_recipe_with_name(FName recipe_name);
    auto replace_session_from_recipe(TArray<FSbxMeshAssemblyRecipePart> recipe_parts,
                                     TArray<FSbxMeshAssemblyRecipeGroup> recipe_groups,
                                     FName output_asset_name,
                                     FName recipe_name,
                                     USbxMeshAssemblyRecipe* current_recipe,
                                     bool recipe_dirty,
                                     FText const& success_status) -> bool;
    void mark_recipe_dirty();
    void notify_session_changed(bool refresh_controls = true);
    [[nodiscard]] auto find_preview_part(UInstancedStaticMeshComponent const* component,
                                         int32 instance_index) const -> int32;
    [[nodiscard]] auto get_preview_part_bounds(int32 part_index) const -> FBox;
    void apply_marquee_selection(TArray<int32> const& matching_part_indices, bool select);
    [[nodiscard]] auto make_part_world_transform(FSbxMeshAssemblyPart const& part) const
        -> FTransform;

    UPROPERTY(Transient)
    TObjectPtr<AActor> preview_actor_;

    UPROPERTY(Transient)
    TObjectPtr<USbxMeshAssemblyRecipe> current_recipe_;

    UPROPERTY(Transient)
    TObjectPtr<USbxMeshAssemblySessionState> session_state_;

    TArray<FSbxMeshAssemblyPart> parts_;
    TArray<FGuid> part_ids_;
    TArray<int32> selected_part_indices_;
    TArray<FSbxMeshPreviewBucket> preview_buckets_;
    TMap<FGuid, int32> part_index_by_id_;
    TMap<FGuid, FSbxMeshPreviewInstanceLocation> preview_location_by_part_id_;
    TArray<TWeakObjectPtr<AActor>> previous_actor_selection_;
    FVector preview_origin_{FVector::ZeroVector};
    FText status_;
    FOnSbxMeshSessionChanged session_changed_;
    TUniquePtr<FScopedTransaction> transform_transaction_;
    int32 selected_part_index_{INDEX_NONE};
    int32 selected_group_index_{INDEX_NONE};
    FGuid snap_target_group_id_;
    int32 snap_target_connector_index_{INDEX_NONE};
    bool changing_selection_{};
    bool recipe_dirty_{};
};
