#pragma once

#include "SandboxEditor/levels/S7LevelAuthoringSession.h"
#include "SandboxEditor/levels/S7LevelSourceSession.h"

#include <Tools/LegacyEdModeWidgetHelpers.h>

#include "S7LevelAuthoringMode.generated.h"

class AS7LevelAuthoringDocument;

DECLARE_MULTICAST_DELEGATE(FOnS7LevelAuthoringChanged);

namespace ioj::editor {}

struct SANDBOXEDITOR_API FS7LevelScriptEditorState {
    FString path{};
    bool source_dirty{};
    bool disk_conflict{};
    bool detached{};
};

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
    [[nodiscard]] auto source_session() -> ioj::editor::FS7LevelSourceSession&;
    [[nodiscard]] auto source_session() const -> ioj::editor::FS7LevelSourceSession const&;
    [[nodiscard]] auto script_editor_state() const -> FS7LevelScriptEditorState;
    [[nodiscard]] auto status() const -> FText const&;
    auto on_changed() -> FOnS7LevelAuthoringChanged&;

    void create_document();
    void adopt_entities();
    void rename_selected_entity(FString id);
    void open_level_config();
    void validate_playable_level();
    void set_up_playable_level();
    auto load_s7() -> bool;
    void reload_s7();
    void set_source_buffer(FString source);
    void import_source();
    void save();
    void export_scene();
  private:
    auto current_level() const -> ULevel*;
    void refresh_document();
    void set_status(FText text);
    void notify_changed(ioj::settings::Settings const* settings = nullptr);

    TWeakObjectPtr<AS7LevelAuthoringDocument> document_{};
    ioj::editor::FS7LevelSourceSession source_session_{};
    FText status_{};
    FOnS7LevelAuthoringChanged changed_{};
    float refresh_elapsed_seconds_{};
};
