#pragma once

#include <Toolkits/BaseToolkit.h>

class IDetailsView;
class SEditableTextBox;
class STextBlock;
class UEdMode;
class US7LevelAuthoringMode;

class FS7LevelAuthoringModeToolkit final : public FModeToolkit {
  public:
    void Init(TSharedPtr<IToolkitHost> const& toolkit_host,
              TWeakObjectPtr<UEdMode> owning_mode) override;
    auto GetToolkitFName() const -> FName override;
    auto GetBaseToolkitName() const -> FText override;
    auto GetInlineContent() const -> TSharedPtr<SWidget> override;
  private:
    void refresh();
    auto create_document() -> FReply;
    auto repair_and_adopt_entities() -> FReply;
    auto rename_selected_entity() -> FReply;
    auto assign_selected_heroes() -> FReply;
    auto assign_selected_must_survive() -> FReply;
    auto assign_selected_required_kills() -> FReply;
    auto clear_selected_objectives() -> FReply;
    auto load_s7() -> FReply;
    auto open_script_editor() -> FReply;
    auto preview_apply() -> FReply;
    auto apply_preview() -> FReply;
    auto save() -> FReply;
    auto save_as() -> FReply;
    auto save_canonical_from_scene() -> FReply;

    TWeakObjectPtr<US7LevelAuthoringMode> mode_{};
    TSharedPtr<IDetailsView> details_{};
    TSharedPtr<SEditableTextBox> entity_id_{};
    TSharedPtr<STextBlock> status_{};
    TSharedPtr<SWidget> content_{};
};
