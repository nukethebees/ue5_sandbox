#pragma once

#include <Toolkits/BaseToolkit.h>

class IDetailsView;
class SEditableTextBox;
class SMultiLineEditableTextBox;
class UEdMode;
class US7LevelAuthoringMode;
class AS7LevelAuthoringDocument;

namespace SlateGenerated {
struct FS7LevelAuthoringModeToolkitBuilder;
}

namespace ioj::settings {
struct Settings;
}

class FS7LevelAuthoringModeToolkit final : public FModeToolkit {
    friend struct SlateGenerated::FS7LevelAuthoringModeToolkitBuilder;
  public:
    void Init(TSharedPtr<IToolkitHost> const& toolkit_host,
              TWeakObjectPtr<UEdMode> owning_mode) override;
    auto GetToolkitFName() const -> FName override;
    auto GetBaseToolkitName() const -> FText override;
    auto GetInlineContent() const -> TSharedPtr<SWidget> override;
    void refresh(ioj::settings::Settings const* settings = nullptr);
  private:
    auto grid_summary() const -> FText;
    auto create_document() -> FReply;
    auto repair_and_adopt_entities() -> FReply;
    auto rename_selected_entity() -> FReply;
    auto open_level_config() -> FReply;
    auto validate_playable_level() -> FReply;
    auto set_up_playable_level() -> FReply;
    auto load_s7() -> FReply;
    auto open_script_editor() -> FReply;
    auto preview_apply() -> FReply;
    auto apply_preview() -> FReply;
    auto save() -> FReply;
    auto save_as() -> FReply;
    auto save_canonical_from_scene() -> FReply;

    TWeakObjectPtr<US7LevelAuthoringMode> mode_{};
    TWeakObjectPtr<AS7LevelAuthoringDocument> displayed_document_{};
    TSharedPtr<IDetailsView> details_{};
    TSharedPtr<SEditableTextBox> entity_id_{};
    TSharedPtr<SMultiLineEditableTextBox> status_{};
    TSharedPtr<SWidget> content_{};
    FText grid_summary_{};
};
