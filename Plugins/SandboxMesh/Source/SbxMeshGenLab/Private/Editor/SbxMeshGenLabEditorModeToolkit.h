#pragma once

#include "Toolkits/BaseToolkit.h"

class IDetailsView;
class STextBlock;
class UEdMode;
class USbxMeshGenLabEditorMode;
template <typename ItemType>
class SListView;

class FSbxMeshGenLabEditorModeToolkit final : public FModeToolkit {
  public:
    void Init(TSharedPtr<IToolkitHost> const& toolkit_host,
              TWeakObjectPtr<UEdMode> owning_mode) override;
    auto GetToolkitFName() const -> FName override;
    auto GetBaseToolkitName() const -> FText override;
    auto GetInlineContent() const -> TSharedPtr<SWidget> override;
  private:
    void on_session_changed(bool refresh_controls);
    void on_property_changed(FPropertyChangedEvent const& event);
    void refresh_part_items();
    auto add_part() -> FReply;
    auto duplicate_part() -> FReply;
    auto remove_part() -> FReply;
    auto new_assembly() -> FReply;
    auto save_recipe() -> FReply;
    auto save_recipe_as() -> FReply;
    auto load_recipe() -> FReply;
    auto save_generated_mesh() -> FReply;

    TWeakObjectPtr<USbxMeshGenLabEditorMode> mode_;
    TArray<TSharedPtr<int32>> part_items_;
    TSharedPtr<SListView<TSharedPtr<int32>>> parts_list_;
    TSharedPtr<STextBlock> recipe_document_text_;
    TSharedPtr<STextBlock> status_text_;
    bool refreshing_{};
};
