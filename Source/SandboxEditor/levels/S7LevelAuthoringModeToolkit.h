#pragma once

#include <Toolkits/BaseToolkit.h>

class IDetailsView;
class SBox;
class SEditableTextBox;
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
    void commit_script_id(FText const& text, ETextCommit::Type commit);
    auto link_script() -> FReply;
    auto open_script_editor() -> FReply;
    auto rebuild_scene() -> FReply;
    auto force_rebuild_scene() -> FReply;
    auto export_scene() -> FReply;
    auto check_scene() -> FReply;

    TWeakObjectPtr<US7LevelAuthoringMode> mode_{};
    TWeakObjectPtr<AS7LevelAuthoringDocument> displayed_document_{};
    TSharedPtr<IDetailsView> details_{};
    TSharedPtr<SBox> script_id_row_{};
    TSharedPtr<SEditableTextBox> entity_id_{};
    TSharedPtr<SWidget> content_{};
};
