#pragma once

#include "SandboxEditor/levels/S7LevelAuthoringSession.h"

#include <Tools/LegacyEdModeWidgetHelpers.h>

#include "S7LevelAuthoringMode.generated.h"

class AS7LevelAuthoringDocument;

UCLASS(Transient)
class SANDBOXEDITOR_API US7LevelAuthoringMode final : public UBaseLegacyWidgetEdMode {
    GENERATED_BODY()
  public:
    static FEditorModeID const mode_id;

    US7LevelAuthoringMode();

    void Enter() override;
    void Exit() override;
    void CreateToolkit() override;
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
    [[nodiscard]] auto status() const -> FText const&;

    void create_document();
    void adopt_entities();
    void rename_selected_entity(FString id);
    void open_level_config();
    void validate_playable_level();
    void set_up_playable_level();
    void load_s7();
    void open_script_editor();
    void import_source(bool root_changed = false);
    void export_scene();
    void export_geometry();
  private:
    auto current_level() const -> ULevel*;
    void refresh_document();
    void set_status(FText text);
    void notify_changed(ioj::settings::Settings const* settings = nullptr);

    TWeakObjectPtr<AS7LevelAuthoringDocument> document_{};
    FText status_{};
    float refresh_elapsed_seconds_{};
};
