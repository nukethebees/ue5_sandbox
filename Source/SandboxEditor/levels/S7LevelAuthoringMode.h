#pragma once

#include "SandboxEditor/levels/S7LevelAuthoringPreview.h"
#include "SandboxEditor/levels/S7LevelAuthoringSession.h"
#include "SandboxEditor/levels/S7LevelReconciliation.h"
#include "SandboxEditor/levels/S7LevelSourceSession.h"

#include <Tools/LegacyEdModeWidgetHelpers.h>

#include "S7LevelAuthoringMode.generated.h"

class AS7LevelAuthoringDocument;

DECLARE_MULTICAST_DELEGATE(FOnS7LevelAuthoringChanged);

namespace ioj::editor {}

struct SANDBOXEDITOR_API FS7LevelScriptEditorState {
    FString path{};
    bool source_dirty{};
    bool unapplied_buffer{};
    bool scene_dirty{};
    bool disk_conflict{};
    bool preview_valid{};
    bool preview_stale{};
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
    void repair_and_adopt_entities();
    void rename_selected_entity(FString id);
    void open_level_config();
    void validate_playable_level();
    void set_up_playable_level();
    auto load_s7() -> bool;
    void reload_s7();
    void set_source_buffer(FString source);
    void preview_apply();
    void apply_preview();
    [[nodiscard]] auto prepare_for_play() -> bool;
    void save();
    void save_as();
    void save_canonical_from_scene();
  private:
    auto current_level() const -> ULevel*;
    void refresh_document();
    void set_status(FText text);
    void update_document_source_path();
    auto prepare_source_replay() -> bool;
    auto apply_current_source(bool require_review) -> bool;

    TWeakObjectPtr<AS7LevelAuthoringDocument> document_{};
    ioj::editor::FS7LevelSourceSession source_session_{};
    TOptional<ioj::editor::FS7LevelAuthoringPreview> preview_{};
    FText status_{};
    FOnS7LevelAuthoringChanged changed_{};
    float refresh_elapsed_seconds_{};
    TOptional<ioj::levels::authoring::SceneSnapshot> observed_scene_{};
    FString observed_scene_source_{};
    int32 observed_actor_count_{-1};
};
