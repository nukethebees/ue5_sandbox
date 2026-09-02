#pragma once

#include "SbxMeshGenLab/MeshAssembly.h"
#include "Tools/LegacyEdModeWidgetHelpers.h"

#include "SbxMeshGenLabEditorMode.generated.h"

class AStaticMeshActor;
class FEditorViewportClient;
class FViewport;
class HHitProxy;
class USbxMeshAssemblyRecipe;
class USbxMeshGenLabSettings;
struct FViewportClick;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSbxMeshSessionChanged, bool);

UCLASS(Transient)
class SBXMESHGENLAB_API USbxMeshGenLabEditorMode final : public UBaseLegacyWidgetEdMode {
    GENERATED_BODY()
  public:
    static FEditorModeID const mode_id;

    USbxMeshGenLabEditorMode();

    void Enter() override;
    void Exit() override;
    void CreateToolkit() override;

    auto UsesTransformWidget() const -> bool override;
    auto ShouldDrawWidget() const -> bool override;
    auto GetWidgetLocation() const -> FVector override;
    auto InputDelta(FEditorViewportClient* viewport_client,
                    FViewport* viewport,
                    FVector& drag,
                    FRotator& rotation,
                    FVector& scale) -> bool override;
    auto EndTracking(FEditorViewportClient* viewport_client, FViewport* viewport) -> bool override;
    auto HandleClick(FEditorViewportClient* viewport_client,
                     HHitProxy* hit_proxy,
                     FViewportClick const& click) -> bool override;
    auto IsSelectionAllowed(AActor* actor, bool selecting) const -> bool override;
    void ActorSelectionChangeNotify() override;

    [[nodiscard]] auto get_settings() const -> USbxMeshGenLabSettings*;
    [[nodiscard]] auto get_parts() const -> TArray<FSbxMeshAssemblyPart> const&;
    [[nodiscard]] auto get_selected_part_index() const -> int32;
    [[nodiscard]] auto get_selected_part_indices() const -> TArray<int32> const&;
    [[nodiscard]] auto can_remove_selected_parts() const -> bool;
    [[nodiscard]] auto get_status() const -> FText const&;
    [[nodiscard]] auto get_recipe_document_text() const -> FText;
    [[nodiscard]] auto has_current_recipe() const -> bool;
    auto on_session_changed() -> FOnSbxMeshSessionChanged&;

    void select_part(int32 part_index);
    void select_parts(TArray<int32> const& part_indices, int32 primary_part_index);
    void select_all_parts();
    void selection_settings_changed();
    void add_part();
    void duplicate_part();
    void remove_part();
    void new_assembly();
    void save_recipe();
    void save_recipe_as();
    void load_recipe();
    void apply_settings();
    void save_generated_mesh();
  private:
    void initialize_session();
    void create_preview_actor(int32 part_index);
    void destroy_preview_actors();
    void refresh_preview_actor(int32 part_index, bool rebuild_mesh);
    void select_preview_actors();
    void sync_selected_part_transforms_from_actors();
    void apply_settings(bool mark_dirty);
    void save_recipe_with_name(FName recipe_name);
    void mark_recipe_dirty();
    void notify_session_changed(bool refresh_controls = true);
    [[nodiscard]] auto find_preview_actor(AActor const* actor) const -> int32;
    [[nodiscard]] auto make_part_world_transform(FSbxMeshAssemblyPart const& part) const
        -> FTransform;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AStaticMeshActor>> preview_actors_;

    UPROPERTY(Transient)
    TObjectPtr<USbxMeshAssemblyRecipe> current_recipe_;

    TArray<FSbxMeshAssemblyPart> parts_;
    TArray<int32> selected_part_indices_;
    TArray<TWeakObjectPtr<AActor>> previous_actor_selection_;
    FVector preview_origin_{FVector::ZeroVector};
    FText status_;
    FOnSbxMeshSessionChanged session_changed_;
    int32 selected_part_index_{INDEX_NONE};
    bool changing_selection_{};
    bool recipe_dirty_{};
};
