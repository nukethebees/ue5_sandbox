#pragma once

#include "SandboxEditor/levels/S7LevelAuthoringSession.h"
#include "SandboxEditor/levels/S7LevelSourceSession.h"

#include <Tools/LegacyEdModeWidgetHelpers.h>

#include "S7LevelAuthoringMode.generated.h"

class AS7LevelAuthoringDocument;

DECLARE_MULTICAST_DELEGATE(FOnS7LevelAuthoringChanged);

UCLASS(Transient)
class SANDBOXEDITOR_API US7LevelAuthoringMode final : public UBaseLegacyWidgetEdMode {
    GENERATED_BODY()
  public:
    static FEditorModeID const mode_id;

    US7LevelAuthoringMode();

    void Enter() override;
    void Exit() override;
    void CreateToolkit() override;
    void Tick(FEditorViewportClient* viewport_client, float delta_time) override;
    void Render(FSceneView const* view,
                FViewport* viewport,
                FPrimitiveDrawInterface* primitive_draw_interface) override;
    void DrawHUD(FEditorViewportClient* viewport_client,
                 FViewport* viewport,
                 FSceneView const* view,
                 FCanvas* canvas) override;

    [[nodiscard]] auto document() const -> AS7LevelAuthoringDocument*;
    [[nodiscard]] auto source_session() -> ml::editor::FS7LevelSourceSession&;
    [[nodiscard]] auto status() const -> FText const&;
    auto on_changed() -> FOnS7LevelAuthoringChanged&;

    void create_document();
    void adopt_entities();
    void assign_selected_heroes();
    void assign_selected_must_survive();
    void assign_selected_required_kills();
    void clear_selected_objectives();
    void load_s7();
    void preview_apply();
    void apply_preview();
    void save();
    void save_as();
    void save_canonical_from_scene();
  private:
    auto current_level() const -> ULevel*;
    void refresh_document();
    void set_status(FText text);
    void update_document_source_path();
    void assign_selected_objective(int32 role);

    TWeakObjectPtr<AS7LevelAuthoringDocument> document_{};
    ml::editor::FS7LevelSourceSession source_session_{};
    TOptional<ml::editor::FS7LevelSyncPlan> preview_{};
    FText status_{};
    FOnS7LevelAuthoringChanged changed_{};
    float refresh_elapsed_seconds_{};
};
