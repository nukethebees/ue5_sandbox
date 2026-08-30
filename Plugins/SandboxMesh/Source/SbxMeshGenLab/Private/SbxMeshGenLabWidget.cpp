#include "SbxMeshGenLab/SbxMeshGenLabWidget.h"

#include "Generation/MeshAssetWriter.h"
#include "SbxMeshGenLab/SbxMeshGenLabSettings.h"

#include "AssetThumbnail.h"
#include "Engine/StaticMesh.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace {
constexpr uint32 thumbnail_size{512};
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
    details_view_->SetObject(settings_);
    details_view_->OnFinishedChangingProperties().AddUObject(this, &ThisClass::on_property_changed);

    thumbnail_pool_ = MakeShared<FAssetThumbnailPool>(1);
    thumbnail_ =
        MakeShared<FAssetThumbnail>(FAssetData{}, thumbnail_size, thumbnail_size, thumbnail_pool_);
    FAssetThumbnailConfig thumbnail_config{};
    thumbnail_config.ThumbnailLabel = EThumbnailLabel::NoLabel;
    thumbnail_config.ShowAssetColor = false;
    auto const preview_widget{thumbnail_->MakeThumbnailWidget(thumbnail_config)};

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
                 SVerticalBox::Slot().FillHeight(
                     1.0f)[SNew(SSplitter) +
                           SSplitter::Slot().Value(0.45f)[details_view_.ToSharedRef()] +
                           SSplitter::Slot().Value(0.55f)[SNew(SBorder).Padding(
                               8.0f)[SNew(SBox)
                                         .WidthOverride(static_cast<float>(thumbnail_size))
                                         .HeightOverride(static_cast<float>(
                                             thumbnail_size))[preview_widget]]]] +
                 SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
                     [SNew(SButton)
                          .Text(NSLOCTEXT("SbxMeshGenLab", "SaveMesh", "Save Generated Mesh"))
                          .ToolTipText(NSLOCTEXT("SbxMeshGenLab",
                                                 "SaveMeshTooltip",
                                                 "Create or replace the configured asset in the "
                                                 "plugin's generated-content directory."))
                          .OnClicked_UObject(this, &ThisClass::save_generated_mesh)] +
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
    thumbnail_.Reset();
    thumbnail_pool_.Reset();
}

void USbxMeshGenLabWidget::on_property_changed(FPropertyChangedEvent const&) {
    if (last_shape_ != settings_->shape) {
        last_shape_ = settings_->shape;
        settings_->load_request(SandboxMesh::make_default_mesh_request(last_shape_));
        details_view_->ForceRefresh();
    }
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
    if (!thumbnail_.IsValid()) {
        return;
    }

    thumbnail_->SetAsset(static_mesh != nullptr ? FAssetData{static_mesh} : FAssetData{});
    thumbnail_->RefreshThumbnail();
}
