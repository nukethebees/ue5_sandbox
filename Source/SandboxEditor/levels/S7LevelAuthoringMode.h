#pragma once

#include "SandboxEditor/levels/S7LevelAuthoringSession.h"
#include <ioj/sim/collision/entity_aabbs.h>
#include <SpaceGamePresentation/simulation/CollisionOverlay.h>

#include <Tools/LegacyEdModeWidgetHelpers.h>

#include "S7LevelAuthoringMode.generated.h"

class AS7LevelAuthoringDocument;
struct FS7LevelEntityBinding;

UCLASS(Transient)
class SANDBOXEDITOR_API US7LevelAuthoringMode final : public UBaseLegacyWidgetEdMode {
    GENERATED_BODY()
  public:
    static FEditorModeID const mode_id;

    US7LevelAuthoringMode();

    void Enter() override;
    void Exit() override;
    void CreateToolkit() override;
    void ActorSelectionChangeNotify() override;
    void PostUndo() override;
    bool RequiresLegacyViewportInteractions() const override { return false; }
    void Tick(FEditorViewportClient* viewport_client, float delta_time) override;
    void Render(FSceneView const* view,
                FViewport* viewport,
                FPrimitiveDrawInterface* primitive_draw_interface) override;
    void DrawHUD(FEditorViewportClient* viewport_client,
                 FViewport* viewport,
                 FSceneView const* view,
                 FCanvas* canvas) override;

    [[nodiscard]] auto document() const -> AS7LevelAuthoringDocument*;

    static auto collision_overlays() -> ioj::FCollisionOverlayVisibility;
    void set_collision_overlays(ioj::FCollisionOverlayVisibility visibility);
    void create_document();
    void rename_selected_entity(FString id);
    auto selected_script_id() const -> FString;
    void link_script();
    void open_script_editor();
    void rebuild_scene();
    void force_rebuild_scene();
    void export_scene();
    void check_scene();
  private:
    void rebuild_scene(ioj::editor::RebuildPolicy policy);
    auto current_level() const -> ULevel*;
    auto selected_binding() const -> FS7LevelEntityBinding*;
    void refresh_document();
    void refresh_collision_preview();
    void apply_collision_preview(ioj::settings::Settings const& settings);
    void draw_collision_preview(FPrimitiveDrawInterface& pdi, ULevel const& level) const;
    void notify_changed();

    static ioj::FCollisionOverlayVisibility collision_overlays_;
    TArray<ioj::FCollisionGridLine> grid_lines_;
    TOptional<ioj::sim::collision::EntityAABBs> entity_bounds_;
    TWeakObjectPtr<AS7LevelAuthoringDocument> document_{};
    TWeakObjectPtr<ULevel> displayed_level_{};
    int32 actor_count_{};
};
