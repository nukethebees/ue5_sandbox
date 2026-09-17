#pragma once

#include <Toolkits/BaseToolkit.h>

class IDetailsView;
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
    auto adopt_entities() -> FReply;
    auto assign_selected_heroes() -> FReply;
    auto assign_selected_must_survive() -> FReply;
    auto assign_selected_required_kills() -> FReply;
    auto clear_selected_objectives() -> FReply;
    auto load_s7() -> FReply;
    auto preview_apply() -> FReply;
    auto apply_preview() -> FReply;
    auto save() -> FReply;
    auto save_as() -> FReply;

    TWeakObjectPtr<US7LevelAuthoringMode> mode_{};
    TSharedPtr<IDetailsView> details_{};
    TSharedPtr<STextBlock> status_{};
    TSharedPtr<SWidget> content_{};
};
