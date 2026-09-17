#pragma once

#include "SandboxEditor/levels/S7LevelAuthoringSession.h"

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
  private:
    auto current_level() const -> ULevel*;
    void refresh_document();
    void set_status(FText text);
    void save_to_path(bool force_dialog);
    void assign_selected_objective(int32 role);
    auto read_current_source(FString& source) const -> bool;

    TWeakObjectPtr<AS7LevelAuthoringDocument> document_{};
    TOptional<ml::editor::FS7LevelSyncPlan> preview_{};
    FText status_{};
    FOnS7LevelAuthoringChanged changed_{};
    float refresh_elapsed_seconds_{};
};
