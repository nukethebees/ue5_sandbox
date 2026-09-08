#pragma once

#include "Toolkits/BaseToolkit.h"
#include "Widgets/Views/STreeView.h"

class IDetailsView;
class STextBlock;
class UEdMode;
class USbxMeshGenLabEditorMode;

enum class ESbxMeshTreeItemKind : uint8 {
    Root,
    Group,
    Part,
};

struct FSbxMeshTreeItem {
    FGuid id;
    ESbxMeshTreeItemKind kind{ESbxMeshTreeItemKind::Part};
    int32 data_index{INDEX_NONE};
    TArray<TSharedPtr<FSbxMeshTreeItem>> children;
};

class FSbxMeshGenLabEditorModeToolkit final : public FModeToolkit {
  public:
    void Init(TSharedPtr<IToolkitHost> const& toolkit_host,
              TWeakObjectPtr<UEdMode> owning_mode) override;
    auto GetToolkitFName() const -> FName override;
    auto GetBaseToolkitName() const -> FText override;
    auto GetInlineContent() const -> TSharedPtr<SWidget> override;
  private:
    using FTreeItem = TSharedPtr<FSbxMeshTreeItem>;

    void on_session_changed(bool refresh_controls);
    void on_property_changed(FPropertyChangedEvent const& event);
    void refresh_tree_items();
    auto generate_tree_row(FTreeItem item, TSharedRef<STableViewBase> const& owner)
        -> TSharedRef<ITableRow>;
    void get_tree_children(FTreeItem item, TArray<FTreeItem>& children) const;
    void select_from_tree(FTreeItem primary_item, ESelectInfo::Type select_info);
    void tree_expansion_changed(FTreeItem item, bool expanded);
    auto begin_tree_drag(FGeometry const& geometry, FPointerEvent const& event, FTreeItem item)
        -> FReply;
    auto can_accept_tree_drop(FDragDropEvent const& event,
                              EItemDropZone drop_zone,
                              FTreeItem target) const -> TOptional<EItemDropZone>;
    auto accept_tree_drop(FDragDropEvent const& event, EItemDropZone drop_zone, FTreeItem target)
        -> FReply;
    void rename_tree_item(FText const& text, ETextCommit::Type commit_type, FTreeItem item);
    [[nodiscard]] auto tree_item_text(FTreeItem item) const -> FText;
    auto add_part() -> FReply;
    auto select_all_parts() -> FReply;
    auto duplicate_part() -> FReply;
    auto remove_part() -> FReply;
    auto create_group() -> FReply;
    auto ungroup() -> FReply;
    auto set_snap_target() -> FReply;
    auto align_connectors() -> FReply;
    auto snap_and_parent() -> FReply;
    auto new_assembly() -> FReply;
    auto save_recipe() -> FReply;
    auto save_recipe_as() -> FReply;
    auto load_recipe() -> FReply;
    auto export_recipe_json() -> FReply;
    auto import_recipe_json() -> FReply;
    auto save_generated_mesh() -> FReply;
    auto show_help() -> FReply;

    TWeakObjectPtr<USbxMeshGenLabEditorMode> mode_;
    TArray<FTreeItem> root_items_;
    TMap<FGuid, FTreeItem> tree_item_by_id_;
    TSet<FGuid> expanded_ids_;
    TSharedPtr<STreeView<FTreeItem>> hierarchy_tree_;
    TSharedPtr<STextBlock> recipe_document_text_;
    TSharedPtr<STextBlock> status_text_;
    bool refreshing_{};
};
