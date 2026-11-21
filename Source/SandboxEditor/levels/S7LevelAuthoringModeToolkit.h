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

class FS7LevelAuthoringModeToolkit final : public FModeToolkit {
    friend struct SlateGenerated::FS7LevelAuthoringModeToolkitBuilder;
  public:
    void Init(TSharedPtr<IToolkitHost> const& toolkit_host,
              TWeakObjectPtr<UEdMode> owning_mode) override;
    auto GetToolkitFName() const -> FName override;
    auto GetBaseToolkitName() const -> FText override;
    auto GetInlineContent() const -> TSharedPtr<SWidget> override;
    void refresh();
  private:
    auto create_document() -> FReply;
    auto rename_selected_entity() -> FReply;
    auto open_level_config() -> FReply;
    auto validate_playable_level() -> FReply;
    auto link_script() -> FReply;
    auto open_script_editor() -> FReply;
    auto rebuild_scene() -> FReply;
    auto export_scene() -> FReply;
    auto check_export() -> FReply;

    TWeakObjectPtr<US7LevelAuthoringMode> mode_{};
    TWeakObjectPtr<AS7LevelAuthoringDocument> displayed_document_{};
    TSharedPtr<IDetailsView> details_{};
    TSharedPtr<SEditableTextBox> entity_id_{};
    TSharedPtr<SMultiLineEditableTextBox> status_{};
    TSharedPtr<SWidget> content_{};
};
