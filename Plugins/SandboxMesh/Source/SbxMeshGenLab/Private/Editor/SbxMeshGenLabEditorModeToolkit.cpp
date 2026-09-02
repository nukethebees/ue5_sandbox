#include "Editor/SbxMeshGenLabEditorModeToolkit.h"

#include "SbxMeshGenLab/SbxMeshGenLabEditorMode.h"
#include "SbxMeshGenLab/SbxMeshGenLabSettings.h"

#include "IDetailsView.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "FSbxMeshGenLabEditorModeToolkit"

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
                          .Text(
                              LOCTEXT("Instructions",
                                      "Select parts here or Ctrl/Shift-click them in the viewport. "
                                      "Use W/E/R to transform the selection around its shared "
                                      "pivot. Properties edit the primary part."))
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
                     [SAssignNew(recipe_document_text_, STextBlock)
                          .Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
                          .AutoWrapText(true)] +
                 SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                     [SNew(STextBlock)
                          .Text(LOCTEXT("Parts", "Assembly Parts"))
                          .Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))] +
                 SVerticalBox::Slot().AutoHeight().MaxHeight(180.0f).Padding(0.0f, 0.0f, 0.0f, 6.0f)
                     [SAssignNew(parts_list_, SListView<TSharedPtr<int32>>)
                          .ListItemsSource(&part_items_)
                          .SelectionMode(ESelectionMode::Multi)
                          .OnGenerateRow_Lambda([this](TSharedPtr<int32> const item,
                                                       TSharedRef<STableViewBase> const& owner) {
                              return SNew(STableRow<TSharedPtr<int32>>,
                                          owner)[SNew(STextBlock).Text_Lambda([this, item]() {
                                  if (!mode_.IsValid() || !item.IsValid() ||
                                      !mode_->get_parts().IsValidIndex(*item)) {
                                      return FText::GetEmpty();
                                  }

                                  auto const& part{mode_->get_parts()[*item]};
                                  auto const* const shape_enum{StaticEnum<ESbxMeshShape>()};
                                  auto const shape_text{
                                      shape_enum == nullptr
                                          ? FText::GetEmpty()
                                          : shape_enum->GetDisplayNameTextByValue(
                                                static_cast<int64>(part.mesh.shape))};
                                  return FText::Format(LOCTEXT("PartEntry", "Part {0} — {1}"),
                                                       FText::AsNumber(*item + 1),
                                                       shape_text);
                              })];
                          })
                          .OnSelectionChanged_Lambda(
                              [this](TSharedPtr<int32> const item, ESelectInfo::Type) {
                                  if (!refreshing_ && mode_.IsValid()) {
                                      select_parts_from_list(item);
                                  }
                              })] +
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
                               .Text(LOCTEXT("DuplicatePart", "Duplicate Selected"))
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
        refresh_part_items();
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
    if (property_name == GET_MEMBER_NAME_CHECKED(USbxMeshGenLabSettings, recipe)) {
        mode_->load_recipe();
    } else if (property_name != GET_MEMBER_NAME_CHECKED(USbxMeshGenLabSettings, recipe_name)) {
        mode_->apply_settings();
    }
}

void FSbxMeshGenLabEditorModeToolkit::refresh_part_items() {
    refreshing_ = true;
    auto const part_count{mode_->get_parts().Num()};
    if (part_items_.Num() != part_count) {
        part_items_.Reset();
        for (int32 part_index{0}; part_index < part_count; ++part_index) {
            part_items_.Add(MakeShared<int32>(part_index));
        }
    }
    parts_list_->RequestListRefresh();

    parts_list_->ClearSelection();
    for (int32 const selected_index : mode_->get_selected_part_indices()) {
        if (part_items_.IsValidIndex(selected_index)) {
            parts_list_->SetItemSelection(part_items_[selected_index], true);
        }
    }
    refreshing_ = false;
}

void FSbxMeshGenLabEditorModeToolkit::select_parts_from_list(TSharedPtr<int32> const primary_item) {
    TArray<int32> selected_indices;
    for (auto const& item : parts_list_->GetSelectedItems()) {
        if (item.IsValid()) {
            selected_indices.Add(*item);
        }
    }

    if (!selected_indices.IsEmpty()) {
        auto const primary_index{primary_item.IsValid() ? *primary_item : selected_indices.Last()};
        mode_->select_parts(selected_indices, primary_index);
    }
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

auto FSbxMeshGenLabEditorModeToolkit::save_generated_mesh() -> FReply {
    mode_->save_generated_mesh();
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
