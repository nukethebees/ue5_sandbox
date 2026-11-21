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
    void rename_selected_entity(FString id);
    void open_level_config();
    void validate_playable_level();
    void link_script();
    void open_script_editor();
    void rebuild_scene();
    void export_scene();
    void check_export();
  private:
    auto current_level() const -> ULevel*;
    void refresh_document();
    void set_status(FText text);
    void notify_changed();

    TWeakObjectPtr<AS7LevelAuthoringDocument> document_{};
    FText status_{};
    TWeakObjectPtr<ULevel> displayed_level_{};
    int32 actor_count_{};
};
