#include "SbxMeshGenLabWidget.h"

#include "Editor/SMeshGenLabViewport.h"
#include "Generation/MeshAssetWriter.h"
#include "SbxMeshGenLab/SbxMeshGenLabSettings.h"

#include "Engine/StaticMesh.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

namespace {

auto get_shape_text(ESbxMeshShape const shape) -> FText {
    auto const* const shape_enum{StaticEnum<ESbxMeshShape>()};
    return shape_enum == nullptr ? FText::GetEmpty()
                                 : shape_enum->GetDisplayNameTextByValue(static_cast<int64>(shape));
}

class SMeshShapeSelector final : public SCompoundWidget {
  public:
    DECLARE_DELEGATE_OneParam(FOnShapeSelected, ESbxMeshShape);

    SLATE_BEGIN_ARGS(SMeshShapeSelector) {}
    SLATE_ARGUMENT(ESbxMeshShape, InitialShape)
    SLATE_EVENT(FOnShapeSelected, OnShapeSelected)
    SLATE_END_ARGS()

    void Construct(FArguments const& arguments) {
        for (auto const shape : {ESbxMeshShape::Box,
                                 ESbxMeshShape::Cylinder,
                                 ESbxMeshShape::Sphere,
                                 ESbxMeshShape::Cone,
                                 ESbxMeshShape::HexTile,
                                 ESbxMeshShape::HexFrame,
                                 ESbxMeshShape::HoneycombPanel}) {
            shape_options_.Add(MakeShared<ESbxMeshShape>(shape));
        }

        selected_shape_ = find_shape(arguments._InitialShape);
        on_shape_selected_ = arguments._OnShapeSelected;

        ChildSlot
            [SAssignNew(shape_combo_box_, SComboBox<TSharedPtr<ESbxMeshShape>>)
                 .OptionsSource(&shape_options_)
                 .InitiallySelectedItem(selected_shape_)
                 .OnGenerateWidget(this, &SMeshShapeSelector::make_shape_widget)
                 .OnSelectionChanged(this, &SMeshShapeSelector::on_selection_changed)
                 .ToolTipText(NSLOCTEXT(
                     "SbxMeshGenLab",
                     "ShapeSelectorTooltip",
                     "Choose a shape. With this control focused, use Up/Down; while hovering, "
                     "use the mouse wheel."))
                     [SNew(STextBlock).Text(this, &SMeshShapeSelector::get_selected_shape_text)]];
    }

    auto OnMouseWheel(FGeometry const&, FPointerEvent const& mouse_event) -> FReply override {
        auto const wheel_delta{mouse_event.GetWheelDelta()};
        if (FMath::IsNearlyZero(wheel_delta)) {
            return FReply::Unhandled();
        }

        step_selection(wheel_delta > 0.0f ? -1 : 1);
        return FReply::Handled();
    }
  private:
    auto find_shape(ESbxMeshShape const shape) const -> TSharedPtr<ESbxMeshShape> {
        auto const* const option{shape_options_.FindByPredicate(
            [shape](TSharedPtr<ESbxMeshShape> const& candidate) { return *candidate == shape; })};
        return option == nullptr ? nullptr : *option;
    }

    auto get_selected_shape_text() const -> FText {
        return selected_shape_.IsValid() ? get_shape_text(*selected_shape_) : FText::GetEmpty();
    }

    auto make_shape_widget(TSharedPtr<ESbxMeshShape> const shape) const -> TSharedRef<SWidget> {
        return SNew(STextBlock).Text(shape.IsValid() ? get_shape_text(*shape) : FText::GetEmpty());
    }

    void on_selection_changed(TSharedPtr<ESbxMeshShape> const shape, ESelectInfo::Type) {
        if (!shape.IsValid() || shape == selected_shape_) {
            return;
        }

        selected_shape_ = shape;
        on_shape_selected_.ExecuteIfBound(*selected_shape_);
    }

    void step_selection(int32 const offset) {
        auto const current_index{shape_options_.IndexOfByKey(selected_shape_)};
        if (current_index == INDEX_NONE) {
            return;
        }

        auto const next_index{FMath::Clamp(current_index + offset, 0, shape_options_.Num() - 1)};
        if (next_index != current_index) {
            shape_combo_box_->SetSelectedItem(shape_options_[next_index]);
        }
    }

    TArray<TSharedPtr<ESbxMeshShape>> shape_options_;
    TSharedPtr<ESbxMeshShape> selected_shape_;
    TSharedPtr<SComboBox<TSharedPtr<ESbxMeshShape>>> shape_combo_box_;
    FOnShapeSelected on_shape_selected_;
};

}

USbxMeshGenLabWidget::USbxMeshGenLabWidget() {
    TabDisplayName = NSLOCTEXT("SbxMeshGenLab", "WidgetLabel", "Mesh Gen Lab");
    bAlwaysReregisterWithWindowsMenu = true;
}

auto USbxMeshGenLabWidget::RebuildWidget() -> TSharedRef<SWidget> {
    settings_ = NewObject<USbxMeshGenLabSettings>(this, NAME_None, RF_Transient);
    settings_->load_request(SandboxMesh::make_default_mesh_request(ESbxMeshShape::Box));
    settings_->asset_name = TEXT("SM_GeneratedAssembly");
    last_shape_ = settings_->shape;

    parts_.Reset();
    selected_part_ = MakeShared<FSbxMeshAssemblyPart>();
    selected_part_->mesh = settings_->to_request();
    selected_part_->transform = settings_->to_transform();
    parts_.Add(selected_part_);

    FDetailsViewArgs details_arguments{};
    details_arguments.bAllowSearch = false;
    details_arguments.bHideSelectionTip = true;
    details_arguments.NameAreaSettings = FDetailsViewArgs::HideNameArea;
    auto& property_editor{
        FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"))};
    details_view_ = property_editor.CreateDetailView(details_arguments);
    details_view_->SetIsPropertyVisibleDelegate(
        FIsPropertyVisible::CreateLambda([](FPropertyAndParent const& property_and_parent) {
            return property_and_parent.Property.GetFName() !=
                   GET_MEMBER_NAME_CHECKED(USbxMeshGenLabSettings, shape);
        }));
    details_view_->SetObject(settings_);
    details_view_->OnFinishedChangingProperties().AddUObject(this, &ThisClass::on_property_changed);

    auto const root{
        SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(12.0f)
                [SNew(SVerticalBox) +
                 SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                     [SNew(STextBlock)
                          .Text(NSLOCTEXT("SbxMeshGenLab", "Title", "Mesh Generation Lab"))
                          .Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))] +
                 SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
                     [SNew(STextBlock)
                          .Text(NSLOCTEXT("SbxMeshGenLab",
                                          "Description",
                                          "Assemble procedural parts with a transient preview, "
                                          "then save them as one static mesh in the plugin's "
                                          "generated-content directory."))
                          .AutoWrapText(true)] +
                 SVerticalBox::Slot().FillHeight(1.0f)
                     [SNew(SSplitter) +
                      SSplitter::Slot().Value(0.45f)
                          [SNew(SVerticalBox) +
                           SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                               [SNew(STextBlock)
                                    .Text(NSLOCTEXT("SbxMeshGenLab", "PartsLabel", "Parts"))
                                    .Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))] +
                           SVerticalBox::Slot().AutoHeight().MaxHeight(140.0f).Padding(
                               0.0f, 0.0f, 0.0f, 6.0f)
                               [SAssignNew(parts_list_, SListView<TSharedPtr<FSbxMeshAssemblyPart>>)
                                    .ListItemsSource(&parts_)
                                    .SelectionMode(ESelectionMode::Single)
                                    .OnGenerateRow_Lambda(
                                        [this](TSharedPtr<FSbxMeshAssemblyPart> const part,
                                               TSharedRef<STableViewBase> const& owner) {
                                            return SNew(
                                                STableRow<TSharedPtr<FSbxMeshAssemblyPart>>,
                                                owner)[SNew(STextBlock).Text_Lambda([this, part]() {
                                                auto const part_index{parts_.IndexOfByKey(part) +
                                                                      1};
                                                return FText::Format(
                                                    NSLOCTEXT("SbxMeshGenLab",
                                                              "PartListEntry",
                                                              "Part {0} — {1}"),
                                                    FText::AsNumber(part_index),
                                                    get_shape_text(part->mesh.shape));
                                            })];
                                        })
                                    .OnSelectionChanged_Lambda(
                                        [this](TSharedPtr<FSbxMeshAssemblyPart> const part,
                                               ESelectInfo::Type) { select_part(part); })] +
                           SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                               [SNew(SHorizontalBox) +
                                SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
                                    [SNew(SButton)
                                         .Text(NSLOCTEXT("SbxMeshGenLab", "AddPart", "Add"))
                                         .OnClicked_UObject(this, &ThisClass::add_part)] +
                                SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
                                    [SNew(SButton)
                                         .Text(NSLOCTEXT(
                                             "SbxMeshGenLab", "DuplicatePart", "Duplicate"))
                                         .OnClicked_UObject(this, &ThisClass::duplicate_part)] +
                                SHorizontalBox::Slot().AutoWidth()
                                    [SNew(SButton)
                                         .Text(NSLOCTEXT("SbxMeshGenLab", "RemovePart", "Remove"))
                                         .IsEnabled_Lambda([this]() { return parts_.Num() > 1; })
                                         .OnClicked_UObject(this, &ThisClass::remove_part)]] +
                           SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                               [SNew(SHorizontalBox) +
                                SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                                        [SNew(STextBlock)
                                             .Text(NSLOCTEXT(
                                                 "SbxMeshGenLab", "ShapeLabel", "Shape"))] +
                                SHorizontalBox::Slot().FillWidth(
                                    1.0f)[SAssignNew(shape_selector_container_, SBox)]] +
                           SVerticalBox::Slot().FillHeight(1.0f)[details_view_.ToSharedRef()]] +
                      SSplitter::Slot().Value(0.55f)[SNew(SBorder).Padding(
                          8.0f)[SNew(SVerticalBox) +
                                SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
                                    [SNew(STextBlock)
                                         .Text(NSLOCTEXT(
                                             "SbxMeshGenLab",
                                             "PreviewControls",
                                             "Left-drag: rotate camera | Shift+left-drag: "
                                             "rotate light | Middle-drag: pan | Right-drag "
                                             "or wheel: zoom | F: focus"))
                                         .AutoWrapText(true)] +
                                SVerticalBox::Slot().FillHeight(1.0f)
                                    [SNew(SBox).MinDesiredWidth(320.0f).MinDesiredHeight(320.0f)
                                         [SAssignNew(preview_viewport_, SMeshGenLabViewport)]]]]] +
                 SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
                     [SNew(SHorizontalBox) +
                      SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
                          [SNew(SButton)
                               .Text(NSLOCTEXT("SbxMeshGenLab", "SaveMesh", "Save Generated Mesh"))
                               .ToolTipText(NSLOCTEXT("SbxMeshGenLab",
                                                      "SaveMeshTooltip",
                                                      "Create or replace the configured asset in "
                                                      "the plugin's generated-content directory."))
                               .OnClicked_UObject(this, &ThisClass::save_generated_mesh)] +
                      SHorizontalBox::Slot().AutoWidth()
                          [SNew(SButton)
                               .Text(NSLOCTEXT("SbxMeshGenLab", "FocusPreview", "Focus Preview"))
                               .OnClicked_UObject(this, &ThisClass::focus_preview)]] +
                 SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
                     [SAssignNew(status_text_, STextBlock).AutoWrapText(true)]]};

    refresh_shape_selector();
    parts_list_->SetSelection(selected_part_);
    update_preview();
    return root;
}

void USbxMeshGenLabWidget::ReleaseSlateResources(bool const release_children) {
    Super::ReleaseSlateResources(release_children);
    set_preview_mesh(nullptr);
    preview_mesh_ = nullptr;
    status_text_.Reset();
    details_view_.Reset();
    shape_selector_container_.Reset();
    parts_list_.Reset();
    preview_viewport_.Reset();
    selected_part_.Reset();
    parts_.Reset();
}

void USbxMeshGenLabWidget::on_property_changed(FPropertyChangedEvent const&) {
    if (last_shape_ != settings_->shape) {
        select_shape(settings_->shape);
        return;
    }
    sync_selected_part_from_settings();
    update_preview();
}

void USbxMeshGenLabWidget::select_part(TSharedPtr<FSbxMeshAssemblyPart> const& part) {
    if (!part.IsValid() || part == selected_part_) {
        return;
    }

    sync_selected_part_from_settings();
    selected_part_ = part;
    auto const asset_name{settings_->asset_name};
    settings_->load_request(selected_part_->mesh);
    settings_->load_transform(selected_part_->transform);
    settings_->asset_name = asset_name;
    last_shape_ = settings_->shape;
    refresh_shape_selector();
    details_view_->ForceRefresh();
}

void USbxMeshGenLabWidget::select_shape(ESbxMeshShape const shape) {
    if (last_shape_ == shape) {
        return;
    }

    auto const asset_name{settings_->asset_name};
    auto const transform{settings_->to_transform()};
    settings_->load_request(SandboxMesh::make_default_mesh_request(shape));
    settings_->load_transform(transform);
    settings_->asset_name = asset_name;
    last_shape_ = shape;
    sync_selected_part_from_settings();
    refresh_parts_list();
    details_view_->ForceRefresh();
    update_preview();
}

auto USbxMeshGenLabWidget::add_part() -> FReply {
    sync_selected_part_from_settings();
    auto part{MakeShared<FSbxMeshAssemblyPart>()};
    part->mesh = SandboxMesh::make_default_mesh_request(ESbxMeshShape::Box);
    parts_.Add(part);
    refresh_parts_list();
    parts_list_->SetSelection(part);
    update_preview();
    return FReply::Handled();
}

auto USbxMeshGenLabWidget::duplicate_part() -> FReply {
    if (!selected_part_.IsValid()) {
        return FReply::Handled();
    }

    sync_selected_part_from_settings();
    auto const part{MakeShared<FSbxMeshAssemblyPart>(*selected_part_)};
    parts_.Add(part);
    refresh_parts_list();
    parts_list_->SetSelection(part);
    update_preview();
    return FReply::Handled();
}

auto USbxMeshGenLabWidget::remove_part() -> FReply {
    if (!selected_part_.IsValid() || parts_.Num() <= 1) {
        return FReply::Handled();
    }

    auto const removed_index{parts_.IndexOfByKey(selected_part_)};
    parts_.RemoveAt(removed_index);
    selected_part_.Reset();
    refresh_parts_list();
    parts_list_->SetSelection(parts_[FMath::Min(removed_index, parts_.Num() - 1)]);
    update_preview();
    return FReply::Handled();
}

auto USbxMeshGenLabWidget::save_generated_mesh() -> FReply {
    sync_selected_part_from_settings();
    TArray<FSbxMeshAssemblyPart> parts;
    parts.Reserve(parts_.Num());
    for (auto const& part : parts_) {
        parts.Add(*part);
        parts.Last().mesh.asset_name = settings_->asset_name;
    }

    auto const validation_error{SandboxMesh::validate_mesh_assembly(parts)};
    if (!validation_error.IsEmpty()) {
        status_text_->SetText(FText::FromString(validation_error));
        return FReply::Handled();
    }

    auto const mesh_data{SandboxMesh::generate_mesh_assembly(parts)};
    auto* const static_mesh{SandboxMesh::write_generated_static_mesh_asset(
        mesh_data, settings_->asset_name, SandboxMesh::describe_mesh_assembly(parts))};
    if (static_mesh == nullptr) {
        status_text_->SetText(NSLOCTEXT(
            "SbxMeshGenLab", "GenerationFailed", "Mesh generation failed; see Output Log."));
        return FReply::Handled();
    }

    preview_mesh_ = nullptr;
    set_preview_mesh(static_mesh);
    status_text_->SetText(
        FText::Format(NSLOCTEXT("SbxMeshGenLab", "GenerationSucceeded", "Saved {0}."),
                      FText::FromString(static_mesh->GetPathName())));
    return FReply::Handled();
}

auto USbxMeshGenLabWidget::focus_preview() -> FReply {
    if (preview_viewport_.IsValid()) {
        preview_viewport_->focus_mesh();
    }
    return FReply::Handled();
}

void USbxMeshGenLabWidget::update_preview() {
    sync_selected_part_from_settings();
    TArray<FSbxMeshAssemblyPart> parts;
    parts.Reserve(parts_.Num());
    for (auto const& part : parts_) {
        parts.Add(*part);
        parts.Last().mesh.asset_name = settings_->asset_name;
    }

    auto const validation_error{SandboxMesh::validate_mesh_assembly(parts)};
    if (!validation_error.IsEmpty()) {
        set_preview_mesh(nullptr);
        preview_mesh_ = nullptr;
        status_text_->SetText(FText::FromString(validation_error));
        return;
    }

    auto const mesh_data{SandboxMesh::generate_mesh_assembly(parts)};
    set_preview_mesh(nullptr);
    preview_mesh_ = SandboxMesh::create_transient_static_mesh(mesh_data);
    if (preview_mesh_ == nullptr) {
        status_text_->SetText(NSLOCTEXT(
            "SbxMeshGenLab", "PreviewFailed", "Failed to build the transient mesh preview."));
        return;
    }

    set_preview_mesh(preview_mesh_);
    status_text_->SetText(FText::Format(
        NSLOCTEXT(
            "SbxMeshGenLab", "PreviewReady", "Preview: {0} parts, {1} vertices, {2} triangles."),
        FText::AsNumber(parts.Num()),
        FText::AsNumber(mesh_data.positions.Num()),
        FText::AsNumber(mesh_data.indices.Num() / 3)));
}

void USbxMeshGenLabWidget::sync_selected_part_from_settings() {
    if (!selected_part_.IsValid() || settings_ == nullptr) {
        return;
    }

    selected_part_->mesh = settings_->to_request();
    selected_part_->transform = settings_->to_transform();
}

void USbxMeshGenLabWidget::refresh_shape_selector() {
    if (!shape_selector_container_.IsValid() || settings_ == nullptr) {
        return;
    }

    shape_selector_container_->SetContent(
        SNew(SMeshShapeSelector)
            .InitialShape(settings_->shape)
            .OnShapeSelected_Lambda([this](ESbxMeshShape const shape) { select_shape(shape); }));
}

void USbxMeshGenLabWidget::refresh_parts_list() {
    if (parts_list_.IsValid()) {
        parts_list_->RequestListRefresh();
    }
}

void USbxMeshGenLabWidget::set_preview_mesh(UStaticMesh* const static_mesh) {
    if (!preview_viewport_.IsValid()) {
        return;
    }

    preview_viewport_->set_mesh(static_mesh);
    if (static_mesh != nullptr) {
        preview_viewport_->focus_mesh();
    }
}
