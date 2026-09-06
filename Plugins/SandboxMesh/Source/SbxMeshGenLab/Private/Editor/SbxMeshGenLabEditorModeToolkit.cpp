#include "Editor/SbxMeshGenLabEditorModeToolkit.h"

#include "SbxMeshGenLab/SbxMeshGenLabEditorMode.h"
#include "SbxMeshGenLab/SbxMeshGenLabSettings.h"

#include "DragAndDrop/DecoratedDragDropOp.h"
#include "IDetailsView.h"
#include "InputCoreTypes.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "FSbxMeshGenLabEditorModeToolkit"

class FSbxMeshTreeDragDropOp final : public FDecoratedDragDropOp {
  public:
    DRAG_DROP_OPERATOR_TYPE(FSbxMeshTreeDragDropOp, FDecoratedDragDropOp)

    static auto create(FGuid const item_id, FText const& text)
        -> TSharedRef<FSbxMeshTreeDragDropOp> {
        auto operation{MakeShared<FSbxMeshTreeDragDropOp>()};
        operation->item_id = item_id;
        operation->DefaultHoverText = text;
        operation->Construct();
        return operation;
    }

    FGuid item_id;
};

void FSbxMeshGenLabEditorModeToolkit::Init(TSharedPtr<IToolkitHost> const& toolkit_host,
                                           TWeakObjectPtr<UEdMode> const owning_mode) {
    FModeToolkit::Init(toolkit_host, owning_mode);

    mode_ = Cast<USbxMeshGenLabEditorMode>(owning_mode.Get());
    check(mode_.IsValid());

    DetailsView->SetObject(mode_->get_settings());
    DetailsView->OnFinishedChangingProperties().AddSP(
        this, &FSbxMeshGenLabEditorModeToolkit::on_property_changed);

    ToolkitWidget =
        SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(8.0f)
                [SNew(SVerticalBox) +
                 SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
                     [SNew(STextBlock)
                          .Text(LOCTEXT(
                              "Instructions",
                              "Select hierarchy nodes here or Ctrl/Shift-click parts in the "
                              "viewport. "
                              "Ctrl+Alt+left-drag box-selects parts. Select a group to "
                              "transform all its descendants. Drag nodes onto a group (or "
                              "Assembly) to reparent them. Double-click a group name to rename "
                              "it. Group connectors are edited below: set a stationary target, "
                              "then select another group and snap its active connector. Use "
                              "W/E/R for transforms and F to frame the selection."))
                          .AutoWrapText(true)] +
                 SVerticalBox::Slot().AutoHeight().Padding(
                     0.0f, 0.0f, 0.0f, 4.0f)[SNew(STextBlock)
                                                 .Text_Lambda([this]() {
                                                     return mode_.IsValid()
                                                              ? mode_->get_snap_target_text()
                                                              : FText::GetEmpty();
                                                 })
                                                 .AutoWrapText(true)] +
                 SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                     [SNew(SHorizontalBox) +
                      SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
                          [SNew(SButton)
                               .Text(LOCTEXT("NewAssembly", "New"))
                               .ToolTipText(
                                   LOCTEXT("NewAssemblyTooltip", "Start a new transient assembly."))
                               .OnClicked(this, &FSbxMeshGenLabEditorModeToolkit::new_assembly)] +
                      SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
                          [SNew(SButton)
                               .Text(LOCTEXT("SaveRecipe", "Save"))
                               .ToolTipText(LOCTEXT("SaveRecipeTooltip",
                                                    "Update the currently loaded recipe."))
                               .IsEnabled_Lambda([this]() {
                                   return mode_.IsValid() && mode_->has_current_recipe();
                               })
                               .OnClicked(this, &FSbxMeshGenLabEditorModeToolkit::save_recipe)] +
                      SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
                          [SNew(SButton)
                               .Text(LOCTEXT("SaveRecipeAs", "Save As"))
                               .ToolTipText(
                                   LOCTEXT("SaveRecipeAsTooltip",
                                           "Save to a recipe asset using the Recipe Name below."))
                               .OnClicked(this, &FSbxMeshGenLabEditorModeToolkit::save_recipe_as)] +
                      SHorizontalBox::Slot().AutoWidth()
                          [SNew(SButton)
                               .Text(LOCTEXT("LoadRecipe", "Reload"))
                               .ToolTipText(
                                   LOCTEXT("LoadRecipeTooltip",
                                           "Discard live changes and reload the current recipe."))
                               .IsEnabled_Lambda([this]() {
                                   return mode_.IsValid() && mode_->has_current_recipe();
                               })
                               .OnClicked(this, &FSbxMeshGenLabEditorModeToolkit::load_recipe)]] +
                 SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                     [SNew(SHorizontalBox) +
                      SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
                          [SNew(SButton)
                               .Text(LOCTEXT("ImportJsonRecipe", "Import JSON"))
                               .ToolTipText(LOCTEXT(
                                   "ImportJsonRecipeTooltip",
                                   "Replace the live assembly with a Sandbox Mesh JSON recipe."))
                               .OnClicked(this,
                                          &FSbxMeshGenLabEditorModeToolkit::import_recipe_json)] +
                      SHorizontalBox::Slot().AutoWidth()
                          [SNew(SButton)
                               .Text(LOCTEXT("ExportJsonRecipe", "Export JSON"))
                               .ToolTipText(
                                   LOCTEXT("ExportJsonRecipeTooltip",
                                           "Export the live assembly as an editable JSON recipe."))
                               .OnClicked(this,
                                          &FSbxMeshGenLabEditorModeToolkit::export_recipe_json)]] +
                 SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                     [SAssignNew(recipe_document_text_, STextBlock)
                          .Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
                          .AutoWrapText(true)] +
                 SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                     [SNew(STextBlock)
                          .Text(LOCTEXT("Hierarchy", "Assembly Hierarchy"))
                          .Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))] +
                 SVerticalBox::Slot().AutoHeight().MaxHeight(280.0f).Padding(0.0f, 0.0f, 0.0f, 6.0f)
                     [SAssignNew(hierarchy_tree_, STreeView<FTreeItem>)
                          .TreeItemsSource(&root_items_)
                          .SelectionMode(ESelectionMode::Multi)
                          .OnGenerateRow(this, &FSbxMeshGenLabEditorModeToolkit::generate_tree_row)
                          .OnGetChildren(this, &FSbxMeshGenLabEditorModeToolkit::get_tree_children)
                          .OnSelectionChanged(this,
                                              &FSbxMeshGenLabEditorModeToolkit::select_from_tree)
                          .OnExpansionChanged(
                              this, &FSbxMeshGenLabEditorModeToolkit::tree_expansion_changed)] +
                 SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                     [SNew(SHorizontalBox) +
                      SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
                          [SNew(SButton)
                               .Text(LOCTEXT("CreateGroup", "Create Group"))
                               .ToolTipText(
                                   LOCTEXT("CreateGroupTooltip",
                                           "Group the selected parts, or wrap the selected group."))
                               .IsEnabled_Lambda([this]() {
                                   return mode_.IsValid() && mode_->can_create_group();
                               })
                               .OnClicked(this, &FSbxMeshGenLabEditorModeToolkit::create_group)] +
                      SHorizontalBox::Slot().AutoWidth()
                          [SNew(SButton)
                               .Text(LOCTEXT("Ungroup", "Ungroup"))
                               .IsEnabled_Lambda(
                                   [this]() { return mode_.IsValid() && mode_->can_ungroup(); })
                               .OnClicked(this, &FSbxMeshGenLabEditorModeToolkit::ungroup)]] +
                 SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                     [SNew(SHorizontalBox) +
                      SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
                          [SNew(SButton)
                               .Text(LOCTEXT("SetSnapTarget", "Set Snap Target"))
                               .ToolTipText(LOCTEXT(
                                   "SetSnapTargetTooltip",
                                   "Remember the active connector on the selected group as the "
                                   "stationary snap target."))
                               .IsEnabled_Lambda([this]() {
                                   return mode_.IsValid() && mode_->can_set_snap_target();
                               })
                               .OnClicked(this,
                                          &FSbxMeshGenLabEditorModeToolkit::set_snap_target)] +
                      SHorizontalBox::Slot().AutoWidth()
                          [SNew(SButton)
                               .Text(LOCTEXT("AlignConnectors", "Align Connectors"))
                               .ToolTipText(LOCTEXT(
                                   "AlignConnectorsTooltip",
                                   "Align the selected group's active connector to the remembered "
                                   "target without changing the hierarchy."))
                               .IsEnabled_Lambda([this]() {
                                   return mode_.IsValid() && mode_->can_snap_selected_group();
                               })
                               .OnClicked(this,
                                          &FSbxMeshGenLabEditorModeToolkit::align_connectors)] +
                      SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
                          [SNew(SButton)
                               .Text(LOCTEXT("SnapAndParent", "Snap and Parent"))
                               .ToolTipText(LOCTEXT("SnapAndParentTooltip",
                                                    "Align the connectors and parent the selected "
                                                    "group to the target "
                                                    "group."))
                               .IsEnabled_Lambda([this]() {
                                   return mode_.IsValid() && mode_->can_snap_selected_group();
                               })
                               .OnClicked(this,
                                          &FSbxMeshGenLabEditorModeToolkit::snap_and_parent)]] +
                 SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                     [SNew(SHorizontalBox) +
                      SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
                          [SNew(SButton)
                               .Text(LOCTEXT("AddPart", "Add"))
                               .OnClicked(this, &FSbxMeshGenLabEditorModeToolkit::add_part)] +
                      SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
                          [SNew(SButton)
                               .Text(LOCTEXT("SelectAllParts", "Select All"))
                               .OnClicked(this,
                                          &FSbxMeshGenLabEditorModeToolkit::select_all_parts)] +
                      SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
                          [SNew(SButton)
                               .Text(LOCTEXT("DuplicatePart", "Duplicate / Repeat"))
                               .ToolTipText(LOCTEXT(
                                   "DuplicatePartTooltip",
                                   "Duplicate the selection using the translation, rotation, and "
                                   "repeat settings below."))
                               .OnClicked(this, &FSbxMeshGenLabEditorModeToolkit::duplicate_part)] +
                      SHorizontalBox::Slot().AutoWidth()
                          [SNew(SButton)
                               .Text(LOCTEXT("RemovePart", "Remove"))
                               .IsEnabled_Lambda([this]() {
                                   return mode_.IsValid() && mode_->can_remove_selected_parts();
                               })
                               .OnClicked(this, &FSbxMeshGenLabEditorModeToolkit::remove_part)]] +
                 SVerticalBox::Slot().FillHeight(1.0f)[DetailsView.ToSharedRef()] +
                 SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
                     [SNew(SButton)
                          .ButtonStyle(FAppStyle::Get(), "PrimaryButton")
                          .Text(LOCTEXT("SaveMesh", "Save Generated Mesh"))
                          .OnClicked(this, &FSbxMeshGenLabEditorModeToolkit::save_generated_mesh)] +
                 SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                     [SAssignNew(status_text_, STextBlock).AutoWrapText(true)]];

    mode_->on_session_changed().AddSP(this, &FSbxMeshGenLabEditorModeToolkit::on_session_changed);
    on_session_changed(true);
}

auto FSbxMeshGenLabEditorModeToolkit::GetToolkitFName() const -> FName {
    return TEXT("SandboxMeshEditorMode");
}

auto FSbxMeshGenLabEditorModeToolkit::GetBaseToolkitName() const -> FText {
    return LOCTEXT("ToolkitName", "Sandbox Mesh");
}

auto FSbxMeshGenLabEditorModeToolkit::GetInlineContent() const -> TSharedPtr<SWidget> {
    return ToolkitWidget;
}

void FSbxMeshGenLabEditorModeToolkit::on_session_changed(bool const refresh_controls) {
    if (!mode_.IsValid()) {
        return;
    }

    if (refresh_controls) {
        refresh_tree_items();
        DetailsView->ForceRefresh();
    }
    recipe_document_text_->SetText(mode_->get_recipe_document_text());
    status_text_->SetText(mode_->get_status());
}

void FSbxMeshGenLabEditorModeToolkit::on_property_changed(FPropertyChangedEvent const& event) {
    if (!mode_.IsValid()) {
        return;
    }

    auto const property_name{event.GetPropertyName()};
    auto const member_property_name{event.GetMemberPropertyName()};
    if (property_name == GET_MEMBER_NAME_CHECKED(USbxMeshGenLabSettings, recipe)) {
        mode_->load_recipe();
    } else if (property_name != GET_MEMBER_NAME_CHECKED(USbxMeshGenLabSettings, recipe_name)) {
        auto const is_selection_pivot{
            member_property_name ==
            GET_MEMBER_NAME_CHECKED(USbxMeshGenLabSettings, selection_pivot)};
        auto const is_connector_selection{
            member_property_name ==
            GET_MEMBER_NAME_CHECKED(USbxMeshGenLabSettings, active_connector_index)};
        auto const is_snap_setting{
            member_property_name ==
            GET_MEMBER_NAME_CHECKED(USbxMeshGenLabSettings, connectors_face_to_face)};
        auto const is_duplicate_setting{
            member_property_name ==
                GET_MEMBER_NAME_CHECKED(USbxMeshGenLabSettings, duplicate_translation_step) ||
            member_property_name ==
                GET_MEMBER_NAME_CHECKED(USbxMeshGenLabSettings, duplicate_rotation_step) ||
            member_property_name ==
                GET_MEMBER_NAME_CHECKED(USbxMeshGenLabSettings, duplicate_repeat_count)};
        if (is_connector_selection || is_snap_setting) {
            return;
        }
        if (is_selection_pivot) {
            mode_->selection_settings_changed();
        } else if (!is_duplicate_setting) {
            mode_->apply_settings();
        }
    }
}

void FSbxMeshGenLabEditorModeToolkit::refresh_tree_items() {
    refreshing_ = true;

    for (auto const& [id, item] : tree_item_by_id_) {
        if (hierarchy_tree_->IsItemExpanded(item)) {
            expanded_ids_.Add(id);
        }
    }

    auto previous_items{MoveTemp(tree_item_by_id_)};
    auto const root{root_items_.IsEmpty() ? MakeShared<FSbxMeshTreeItem>() : root_items_[0]};
    root->kind = ESbxMeshTreeItemKind::Root;
    root->children.Reset();
    root_items_ = {root};

    auto const& groups{mode_->get_groups()};
    auto const group_count{groups.Num()};
    for (int32 group_index{}; group_index < group_count; ++group_index) {
        auto item{previous_items.FindRef(groups[group_index].id)};
        if (!item.IsValid() || item->kind != ESbxMeshTreeItemKind::Group) {
            item = MakeShared<FSbxMeshTreeItem>();
        }
        item->id = groups[group_index].id;
        item->kind = ESbxMeshTreeItemKind::Group;
        item->data_index = group_index;
        item->children.Reset();
        tree_item_by_id_.Add(item->id, item);
    }

    auto const& recipe_parts{mode_->get_recipe_parts()};
    auto const part_count{recipe_parts.Num()};
    for (int32 part_index{}; part_index < part_count; ++part_index) {
        auto item{previous_items.FindRef(recipe_parts[part_index].id)};
        if (!item.IsValid() || item->kind != ESbxMeshTreeItemKind::Part) {
            item = MakeShared<FSbxMeshTreeItem>();
        }
        item->id = recipe_parts[part_index].id;
        item->kind = ESbxMeshTreeItemKind::Part;
        item->data_index = part_index;
        item->children.Reset();
        tree_item_by_id_.Add(item->id, item);
    }

    for (auto const& group : groups) {
        auto const item{tree_item_by_id_.FindChecked(group.id)};
        auto const* const parent{tree_item_by_id_.Find(group.parent_id)};
        if (parent != nullptr && (*parent)->kind == ESbxMeshTreeItemKind::Group) {
            (*parent)->children.Add(item);
        } else {
            root->children.Add(item);
        }
    }
    for (auto const& part : recipe_parts) {
        auto const item{tree_item_by_id_.FindChecked(part.id)};
        auto const* const parent{tree_item_by_id_.Find(part.parent_id)};
        if (parent != nullptr && (*parent)->kind == ESbxMeshTreeItemKind::Group) {
            (*parent)->children.Add(item);
        } else {
            root->children.Add(item);
        }
    }

    hierarchy_tree_->RequestTreeRefresh();
    hierarchy_tree_->SetItemExpansion(root, true);
    for (FGuid const id : expanded_ids_) {
        if (auto const* const item{tree_item_by_id_.Find(id)}; item != nullptr) {
            hierarchy_tree_->SetItemExpansion(*item, true);
        }
    }

    hierarchy_tree_->ClearSelection();
    auto const expand_ancestors = [this, &groups](FGuid parent_id) {
        while (parent_id.IsValid()) {
            auto const* const parent_item{tree_item_by_id_.Find(parent_id)};
            if (parent_item == nullptr) {
                break;
            }
            expanded_ids_.Add(parent_id);
            hierarchy_tree_->SetItemExpansion(*parent_item, true);
            auto const parent_index{
                groups.IndexOfByPredicate([parent_id](FSbxMeshAssemblyRecipeGroup const& group) {
                    return group.id == parent_id;
                })};
            if (!groups.IsValidIndex(parent_index)) {
                break;
            }
            parent_id = groups[parent_index].parent_id;
        }
    };
    auto const selected_group_index{mode_->get_selected_group_index()};
    if (groups.IsValidIndex(selected_group_index)) {
        expand_ancestors(groups[selected_group_index].parent_id);
        hierarchy_tree_->SetItemSelection(
            tree_item_by_id_.FindChecked(groups[selected_group_index].id), true);
    } else {
        for (int32 const selected_index : mode_->get_selected_part_indices()) {
            if (recipe_parts.IsValidIndex(selected_index)) {
                expand_ancestors(recipe_parts[selected_index].parent_id);
                hierarchy_tree_->SetItemSelection(
                    tree_item_by_id_.FindChecked(recipe_parts[selected_index].id), true);
            }
        }
    }
    refreshing_ = false;
}

auto FSbxMeshGenLabEditorModeToolkit::generate_tree_row(FTreeItem const item,
                                                        TSharedRef<STableViewBase> const& owner)
    -> TSharedRef<ITableRow> {
    TSharedRef<SWidget> content{
        SNew(STextBlock).Text_Lambda([this, item]() { return tree_item_text(item); })};
    if (item->kind == ESbxMeshTreeItemKind::Group) {
        content =
            SNew(SInlineEditableTextBlock)
                .Text_Lambda([this, item]() { return tree_item_text(item); })
                .OnTextCommitted(this, &FSbxMeshGenLabEditorModeToolkit::rename_tree_item, item);
    }

    return SNew(STableRow<FTreeItem>, owner)
        .OnDragDetected(this, &FSbxMeshGenLabEditorModeToolkit::begin_tree_drag, item)
        .OnCanAcceptDrop(this, &FSbxMeshGenLabEditorModeToolkit::can_accept_tree_drop)
        .OnAcceptDrop(this, &FSbxMeshGenLabEditorModeToolkit::accept_tree_drop)[content];
}

void FSbxMeshGenLabEditorModeToolkit::get_tree_children(FTreeItem const item,
                                                        TArray<FTreeItem>& children) const {
    if (item.IsValid()) {
        children = item->children;
    }
}

void FSbxMeshGenLabEditorModeToolkit::select_from_tree(FTreeItem const primary_item,
                                                       ESelectInfo::Type) {
    if (refreshing_ || !mode_.IsValid()) {
        return;
    }
    if (!primary_item.IsValid() || primary_item->kind == ESbxMeshTreeItemKind::Root) {
        mode_->SelectNone();
        return;
    }

    TArray<FGuid> selected_ids;
    for (auto const& item : hierarchy_tree_->GetSelectedItems()) {
        if (item.IsValid() && item->kind != ESbxMeshTreeItemKind::Root) {
            selected_ids.Add(item->id);
        }
    }
    mode_->select_nodes(selected_ids, primary_item->id);
}

void FSbxMeshGenLabEditorModeToolkit::tree_expansion_changed(FTreeItem const item,
                                                             bool const expanded) {
    if (refreshing_ || !item.IsValid() || !item->id.IsValid()) {
        return;
    }
    if (expanded) {
        expanded_ids_.Add(item->id);
    } else {
        expanded_ids_.Remove(item->id);
    }
}

auto FSbxMeshGenLabEditorModeToolkit::begin_tree_drag(FGeometry const&,
                                                      FPointerEvent const& event,
                                                      FTreeItem const item) -> FReply {
    if (!item.IsValid() || item->kind == ESbxMeshTreeItemKind::Root ||
        !event.IsMouseButtonDown(EKeys::LeftMouseButton)) {
        return FReply::Unhandled();
    }
    return FReply::Handled().BeginDragDrop(FSbxMeshTreeDragDropOp::create(
        item->id, FText::Format(LOCTEXT("MoveHierarchyNode", "Move {0}"), tree_item_text(item))));
}

auto FSbxMeshGenLabEditorModeToolkit::can_accept_tree_drop(FDragDropEvent const& event,
                                                           EItemDropZone,
                                                           FTreeItem const target) const
    -> TOptional<EItemDropZone> {
    auto const operation{event.GetOperationAs<FSbxMeshTreeDragDropOp>()};
    if (!operation.IsValid() || !target.IsValid() || target->kind == ESbxMeshTreeItemKind::Part ||
        operation->item_id == target->id) {
        return {};
    }
    return EItemDropZone::OntoItem;
}

auto FSbxMeshGenLabEditorModeToolkit::accept_tree_drop(FDragDropEvent const& event,
                                                       EItemDropZone,
                                                       FTreeItem const target) -> FReply {
    auto const operation{event.GetOperationAs<FSbxMeshTreeDragDropOp>()};
    if (!mode_.IsValid() || !operation.IsValid() || !target.IsValid()) {
        return FReply::Unhandled();
    }
    auto const parent_id{target->kind == ESbxMeshTreeItemKind::Root ? FGuid{} : target->id};
    return mode_->reparent_node(operation->item_id, parent_id) ? FReply::Handled()
                                                               : FReply::Unhandled();
}

void FSbxMeshGenLabEditorModeToolkit::rename_tree_item(FText const& text,
                                                       ETextCommit::Type,
                                                       FTreeItem const item) {
    if (mode_.IsValid() && item.IsValid() && item->kind == ESbxMeshTreeItemKind::Group) {
        mode_->rename_group(item->id, FName{text.ToString().TrimStartAndEnd()});
    }
}

auto FSbxMeshGenLabEditorModeToolkit::tree_item_text(FTreeItem const item) const -> FText {
    if (!mode_.IsValid() || !item.IsValid()) {
        return FText::GetEmpty();
    }
    if (item->kind == ESbxMeshTreeItemKind::Root) {
        return LOCTEXT("AssemblyRoot", "Assembly");
    }
    if (item->kind == ESbxMeshTreeItemKind::Group) {
        auto const& groups{mode_->get_groups()};
        return groups.IsValidIndex(item->data_index)
                 ? FText::FromName(groups[item->data_index].name)
                 : FText::GetEmpty();
    }

    auto const& parts{mode_->get_parts()};
    if (!parts.IsValidIndex(item->data_index)) {
        return FText::GetEmpty();
    }
    auto const* const shape_enum{StaticEnum<ESbxMeshShape>()};
    auto const shape_text{shape_enum == nullptr
                              ? FText::GetEmpty()
                              : shape_enum->GetDisplayNameTextByValue(
                                    static_cast<int64>(parts[item->data_index].mesh.shape))};
    return FText::Format(
        LOCTEXT("PartEntry", "Part {0} — {1}"), FText::AsNumber(item->data_index + 1), shape_text);
}

auto FSbxMeshGenLabEditorModeToolkit::add_part() -> FReply {
    mode_->add_part();
    return FReply::Handled();
}

auto FSbxMeshGenLabEditorModeToolkit::select_all_parts() -> FReply {
    mode_->select_all_parts();
    return FReply::Handled();
}

auto FSbxMeshGenLabEditorModeToolkit::duplicate_part() -> FReply {
    mode_->duplicate_part();
    return FReply::Handled();
}

auto FSbxMeshGenLabEditorModeToolkit::remove_part() -> FReply {
    mode_->remove_part();
    return FReply::Handled();
}

auto FSbxMeshGenLabEditorModeToolkit::create_group() -> FReply {
    mode_->create_group();
    return FReply::Handled();
}

auto FSbxMeshGenLabEditorModeToolkit::ungroup() -> FReply {
    mode_->ungroup();
    return FReply::Handled();
}

auto FSbxMeshGenLabEditorModeToolkit::set_snap_target() -> FReply {
    mode_->set_snap_target();
    return FReply::Handled();
}

auto FSbxMeshGenLabEditorModeToolkit::align_connectors() -> FReply {
    mode_->align_connectors();
    return FReply::Handled();
}

auto FSbxMeshGenLabEditorModeToolkit::snap_and_parent() -> FReply {
    mode_->snap_and_parent();
    return FReply::Handled();
}

auto FSbxMeshGenLabEditorModeToolkit::new_assembly() -> FReply {
    mode_->new_assembly();
    return FReply::Handled();
}

auto FSbxMeshGenLabEditorModeToolkit::save_recipe() -> FReply {
    mode_->save_recipe();
    return FReply::Handled();
}

auto FSbxMeshGenLabEditorModeToolkit::save_recipe_as() -> FReply {
    mode_->save_recipe_as();
    return FReply::Handled();
}

auto FSbxMeshGenLabEditorModeToolkit::load_recipe() -> FReply {
    mode_->load_recipe();
    return FReply::Handled();
}

auto FSbxMeshGenLabEditorModeToolkit::import_recipe_json() -> FReply {
    mode_->import_recipe_json();
    return FReply::Handled();
}

auto FSbxMeshGenLabEditorModeToolkit::export_recipe_json() -> FReply {
    mode_->export_recipe_json();
    return FReply::Handled();
}

auto FSbxMeshGenLabEditorModeToolkit::save_generated_mesh() -> FReply {
    mode_->save_generated_mesh();
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
