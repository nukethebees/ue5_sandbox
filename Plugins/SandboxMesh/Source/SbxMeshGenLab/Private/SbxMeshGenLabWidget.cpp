#include "SbxMeshGenLab/SbxMeshGenLabWidget.h"

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

namespace {

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

    auto get_shape_text(ESbxMeshShape const shape) const -> FText {
        auto const* const shape_enum{StaticEnum<ESbxMeshShape>()};
        return shape_enum == nullptr
                 ? FText::GetEmpty()
                 : shape_enum->GetDisplayNameTextByValue(static_cast<int64>(shape));
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
    last_shape_ = settings_->shape;

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
                                          "Edit a procedural mesh with a transient preview, then "
                                          "save it into the plugin's generated-content directory."))
                          .AutoWrapText(true)] +
                 SVerticalBox::Slot().FillHeight(1.0f)
                     [SNew(SSplitter) +
                      SSplitter::Slot().Value(0.45f)
                          [SNew(SVerticalBox) +
                           SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                               [SNew(SHorizontalBox) +
                                SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    .Padding(0.0f, 0.0f, 8.0f, 0.0f)[SNew(STextBlock)
                                                                         .Text(NSLOCTEXT(
                                                                             "SbxMeshGenLab",
                                                                             "ShapeLabel",
                                                                             "Shape"))] +
                                SHorizontalBox::Slot().FillWidth(1.0f)
                                    [SNew(SMeshShapeSelector)
                                         .InitialShape(settings_->shape)
                                         .OnShapeSelected_Lambda([this](ESbxMeshShape const shape) {
                                             select_shape(shape);
                                         })]] +
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

    update_preview();
    return root;
}

void USbxMeshGenLabWidget::ReleaseSlateResources(bool const release_children) {
    Super::ReleaseSlateResources(release_children);
    set_preview_mesh(nullptr);
    preview_mesh_ = nullptr;
    status_text_.Reset();
    details_view_.Reset();
    preview_viewport_.Reset();
}

void USbxMeshGenLabWidget::on_property_changed(FPropertyChangedEvent const&) {
    if (last_shape_ != settings_->shape) {
        select_shape(settings_->shape);
        return;
    }
    update_preview();
}

void USbxMeshGenLabWidget::select_shape(ESbxMeshShape const shape) {
    if (last_shape_ == shape) {
        return;
    }

    last_shape_ = shape;
    settings_->load_request(SandboxMesh::make_default_mesh_request(shape));
    details_view_->ForceRefresh();
    update_preview();
}

auto USbxMeshGenLabWidget::save_generated_mesh() -> FReply {
    auto const request{settings_->to_request()};
    auto const validation_error{SandboxMesh::validate_mesh_request(request)};
    if (!validation_error.IsEmpty()) {
        status_text_->SetText(FText::FromString(validation_error));
        return FReply::Handled();
    }

    auto const mesh_data{SandboxMesh::generate_mesh(request)};
    auto* const static_mesh{SandboxMesh::write_generated_static_mesh_asset(
        mesh_data, request.asset_name, SandboxMesh::describe_mesh_request(request))};
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
    auto const request{settings_->to_request()};
    auto const validation_error{SandboxMesh::validate_mesh_request(request)};
    if (!validation_error.IsEmpty()) {
        set_preview_mesh(nullptr);
        preview_mesh_ = nullptr;
        status_text_->SetText(FText::FromString(validation_error));
        return;
    }

    auto const mesh_data{SandboxMesh::generate_mesh(request)};
    set_preview_mesh(nullptr);
    preview_mesh_ = SandboxMesh::create_transient_static_mesh(mesh_data);
    if (preview_mesh_ == nullptr) {
        status_text_->SetText(NSLOCTEXT(
            "SbxMeshGenLab", "PreviewFailed", "Failed to build the transient mesh preview."));
        return;
    }

    set_preview_mesh(preview_mesh_);
    status_text_->SetText(FText::Format(
        NSLOCTEXT("SbxMeshGenLab", "PreviewReady", "Preview: {0} vertices, {1} triangles."),
        FText::AsNumber(mesh_data.positions.Num()),
        FText::AsNumber(mesh_data.indices.Num() / 3)));
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
