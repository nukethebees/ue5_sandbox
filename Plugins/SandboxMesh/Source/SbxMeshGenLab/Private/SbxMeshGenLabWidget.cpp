#include "SbxMeshGenLab/SbxMeshGenLabWidget.h"

#include "Generation/MeshAssetWriter.h"
#include "SbxMeshGenLab/BoxGenerator.h"
#include "SbxMeshGenLab/ConeGenerator.h"
#include "SbxMeshGenLab/CylinderGenerator.h"
#include "SbxMeshGenLab/HexFrameGenerator.h"
#include "SbxMeshGenLab/SphereGenerator.h"

#include "AssetThumbnail.h"
#include "Engine/StaticMesh.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace {
FName const generated_box_asset_name{TEXT("SM_GeneratedBox")};
FName const generated_cone_asset_name{TEXT("SM_GeneratedCone")};
FName const generated_cylinder_asset_name{TEXT("SM_GeneratedCylinder")};
FName const generated_hex_frame_asset_name{TEXT("SM_GeneratedHexFrame")};
FName const generated_sphere_asset_name{TEXT("SM_GeneratedSphere")};
constexpr uint32 thumbnail_size{256};
constexpr float minimum_dimension{1.0f};
constexpr float maximum_dimension{100000.0f};
constexpr float maximum_slider_dimension{1000.0f};
constexpr int32 minimum_radial_segments{3};
constexpr int32 maximum_radial_segments{256};
constexpr int32 maximum_slider_radial_segments{128};

auto make_float_control(FText const& label,
                        TAttribute<float> value,
                        SSpinBox<float>::FOnValueChanged on_value_changed) -> TSharedRef<SWidget> {
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot()
               .AutoWidth()
               .VAlign(VAlign_Center)
               .Padding(0.0f, 0.0f, 6.0f, 0.0f)[SNew(STextBlock).Text(label)] +
           SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SSpinBox<float>)
                                                      .Value(value)
                                                      .MinValue(minimum_dimension)
                                                      .MaxValue(maximum_dimension)
                                                      .MinSliderValue(minimum_dimension)
                                                      .MaxSliderValue(maximum_slider_dimension)
                                                      .Delta(1.0f)
                                                      .OnValueChanged(on_value_changed)];
}

auto make_segment_control(FText const& label,
                          TAttribute<int32> value,
                          SSpinBox<int32>::FOnValueChanged on_value_changed,
                          int32 const minimum_segments = minimum_radial_segments)
    -> TSharedRef<SWidget> {
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot()
               .AutoWidth()
               .VAlign(VAlign_Center)
               .Padding(0.0f, 0.0f, 6.0f, 0.0f)[SNew(STextBlock).Text(label)] +
           SHorizontalBox::Slot().FillWidth(
               1.0f)[SNew(SSpinBox<int32>)
                         .Value(value)
                         .MinValue(minimum_segments)
                         .MaxValue(maximum_radial_segments)
                         .MinSliderValue(minimum_segments)
                         .MaxSliderValue(maximum_slider_radial_segments)
                         .Delta(1)
                         .OnValueChanged(on_value_changed)];
}
}

USbxMeshGenLabWidget::USbxMeshGenLabWidget() {
    TabDisplayName = NSLOCTEXT("SbxMeshGenLab", "TabName", "Mesh Gen Lab");
    bAlwaysReregisterWithWindowsMenu = true;
}

auto USbxMeshGenLabWidget::RebuildWidget() -> TSharedRef<SWidget> {
    thumbnail_pool_ = MakeShared<FAssetThumbnailPool>(1);
    thumbnail_ =
        MakeShared<FAssetThumbnail>(FAssetData{}, thumbnail_size, thumbnail_size, thumbnail_pool_);

    FAssetThumbnailConfig thumbnail_config{};
    thumbnail_config.ThumbnailLabel = EThumbnailLabel::NoLabel;
    thumbnail_config.ShowAssetColor = false;

    auto const preview_widget{thumbnail_->MakeThumbnailWidget(thumbnail_config)};
    auto const generated_object_path{
        SandboxMesh::get_generated_asset_object_path(generated_asset_name())};
    set_preview_mesh(LoadObject<UStaticMesh>(nullptr, *generated_object_path));

    auto const box_controls{
        SNew(SVerticalBox).Visibility_Lambda([this] {
            return selected_shape_ == ESbxMeshShape::Box ? EVisibility::Visible
                                                         : EVisibility::Collapsed;
        }) +
        SVerticalBox::Slot().AutoHeight().Padding(
            0.0f,
            0.0f,
            0.0f,
            4.0f)[SNew(STextBlock)
                      .Text(NSLOCTEXT("SbxMeshGenLab", "BoxDimensions", "Dimensions (cm)"))] +
        SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[make_float_control(
            NSLOCTEXT("SbxMeshGenLab", "DimensionX", "X"),
            TAttribute<float>::CreateLambda([this] { return box_dimensions_.X; }),
            SSpinBox<float>::FOnValueChanged::CreateLambda(
                [this](float const value) { box_dimensions_.X = value; }))] +
        SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[make_float_control(
            NSLOCTEXT("SbxMeshGenLab", "DimensionY", "Y"),
            TAttribute<float>::CreateLambda([this] { return box_dimensions_.Y; }),
            SSpinBox<float>::FOnValueChanged::CreateLambda(
                [this](float const value) { box_dimensions_.Y = value; }))] +
        SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)[make_float_control(
            NSLOCTEXT("SbxMeshGenLab", "DimensionZ", "Z"),
            TAttribute<float>::CreateLambda([this] { return box_dimensions_.Z; }),
            SSpinBox<float>::FOnValueChanged::CreateLambda(
                [this](float const value) { box_dimensions_.Z = value; }))]};

    auto const cylinder_controls{
        SNew(SVerticalBox).Visibility_Lambda([this] {
            return selected_shape_ == ESbxMeshShape::Cylinder ? EVisibility::Visible
                                                              : EVisibility::Collapsed;
        }) +
        SVerticalBox::Slot().AutoHeight().Padding(
            0.0f,
            0.0f,
            0.0f,
            4.0f)[SNew(STextBlock)
                      .Text(NSLOCTEXT("SbxMeshGenLab", "CylinderDimensions", "Cylinder (cm)"))] +
        SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[make_float_control(
            NSLOCTEXT("SbxMeshGenLab", "CylinderRadius", "Radius"),
            TAttribute<float>::CreateLambda([this] { return cylinder_radius_; }),
            SSpinBox<float>::FOnValueChanged::CreateLambda(
                [this](float const value) { cylinder_radius_ = value; }))] +
        SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[make_float_control(
            NSLOCTEXT("SbxMeshGenLab", "CylinderHeight", "Height"),
            TAttribute<float>::CreateLambda([this] { return cylinder_height_; }),
            SSpinBox<float>::FOnValueChanged::CreateLambda(
                [this](float const value) { cylinder_height_ = value; }))] +
        SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)[make_segment_control(
            NSLOCTEXT("SbxMeshGenLab", "CylinderRadialSegments", "Radial Segments"),
            TAttribute<int32>::CreateLambda([this] { return cylinder_radial_segments_; }),
            SSpinBox<int32>::FOnValueChanged::CreateLambda(
                [this](int32 const value) { cylinder_radial_segments_ = value; }))]};

    auto const sphere_controls{
        SNew(SVerticalBox).Visibility_Lambda([this] {
            return selected_shape_ == ESbxMeshShape::Sphere ? EVisibility::Visible
                                                            : EVisibility::Collapsed;
        }) +
        SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
            [SNew(STextBlock).Text(NSLOCTEXT("SbxMeshGenLab", "SphereDimensions", "Sphere (cm)"))] +
        SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[make_float_control(
            NSLOCTEXT("SbxMeshGenLab", "SphereRadius", "Radius"),
            TAttribute<float>::CreateLambda([this] { return sphere_radius_; }),
            SSpinBox<float>::FOnValueChanged::CreateLambda(
                [this](float const value) { sphere_radius_ = value; }))] +
        SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[make_segment_control(
            NSLOCTEXT("SbxMeshGenLab", "SphereLongitudeSegments", "Longitude Segments"),
            TAttribute<int32>::CreateLambda([this] { return sphere_longitude_segments_; }),
            SSpinBox<int32>::FOnValueChanged::CreateLambda(
                [this](int32 const value) { sphere_longitude_segments_ = value; }))] +
        SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)[make_segment_control(
            NSLOCTEXT("SbxMeshGenLab", "SphereLatitudeSegments", "Latitude Segments"),
            TAttribute<int32>::CreateLambda([this] { return sphere_latitude_segments_; }),
            SSpinBox<int32>::FOnValueChanged::CreateLambda(
                [this](int32 const value) { sphere_latitude_segments_ = value; }),
            2)]};

    auto const cone_controls{
        SNew(SVerticalBox).Visibility_Lambda([this] {
            return selected_shape_ == ESbxMeshShape::Cone ? EVisibility::Visible
                                                          : EVisibility::Collapsed;
        }) +
        SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
            [SNew(STextBlock).Text(NSLOCTEXT("SbxMeshGenLab", "ConeDimensions", "Cone (cm)"))] +
        SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[make_float_control(
            NSLOCTEXT("SbxMeshGenLab", "ConeRadius", "Radius"),
            TAttribute<float>::CreateLambda([this] { return cone_radius_; }),
            SSpinBox<float>::FOnValueChanged::CreateLambda(
                [this](float const value) { cone_radius_ = value; }))] +
        SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[make_float_control(
            NSLOCTEXT("SbxMeshGenLab", "ConeHeight", "Height"),
            TAttribute<float>::CreateLambda([this] { return cone_height_; }),
            SSpinBox<float>::FOnValueChanged::CreateLambda(
                [this](float const value) { cone_height_ = value; }))] +
        SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)[make_segment_control(
            NSLOCTEXT("SbxMeshGenLab", "ConeRadialSegments", "Radial Segments"),
            TAttribute<int32>::CreateLambda([this] { return cone_radial_segments_; }),
            SSpinBox<int32>::FOnValueChanged::CreateLambda(
                [this](int32 const value) { cone_radial_segments_ = value; }))]};

    auto const hex_frame_controls{
        SNew(SVerticalBox).Visibility_Lambda([this] {
            return selected_shape_ == ESbxMeshShape::HexFrame ? EVisibility::Visible
                                                              : EVisibility::Collapsed;
        }) +
        SVerticalBox::Slot().AutoHeight().Padding(
            0.0f,
            0.0f,
            0.0f,
            4.0f)[SNew(STextBlock)
                      .Text(NSLOCTEXT("SbxMeshGenLab", "HexFrameDimensions", "Hex Frame (cm)"))] +
        SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[make_float_control(
            NSLOCTEXT("SbxMeshGenLab", "HexFrameOuterRadius", "Outer Radius"),
            TAttribute<float>::CreateLambda([this] { return hex_frame_outer_radius_; }),
            SSpinBox<float>::FOnValueChanged::CreateLambda(
                [this](float const value) { hex_frame_outer_radius_ = value; }))] +
        SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[make_float_control(
            NSLOCTEXT("SbxMeshGenLab", "HexFrameWallThickness", "Wall Thickness"),
            TAttribute<float>::CreateLambda([this] { return hex_frame_wall_thickness_; }),
            SSpinBox<float>::FOnValueChanged::CreateLambda(
                [this](float const value) { hex_frame_wall_thickness_ = value; }))] +
        SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[make_float_control(
            NSLOCTEXT("SbxMeshGenLab", "HexFrameDepth", "Depth"),
            TAttribute<float>::CreateLambda([this] { return hex_frame_depth_; }),
            SSpinBox<float>::FOnValueChanged::CreateLambda(
                [this](float const value) { hex_frame_depth_ = value; }))] +
        SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
            [SNew(SCheckBox)
                 .IsChecked_Lambda([this] {
                     return hex_frame_pointy_top_ ? ECheckBoxState::Checked
                                                  : ECheckBoxState::Unchecked;
                 })
                 .OnCheckStateChanged_Lambda([this](ECheckBoxState const state) {
                     hex_frame_pointy_top_ = state == ECheckBoxState::Checked;
                 })[SNew(STextBlock)
                        .Text(NSLOCTEXT("SbxMeshGenLab", "HexFramePointyTop", "Pointy Top"))]]};

    return SNew(SBorder)
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
                                      "Generate disposable procedural static meshes."))] +
             SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
                 [SNew(SSegmentedControl<ESbxMeshShape>)
                      .Value_Lambda([this] { return selected_shape_; })
                      .OnValueChanged_UObject(this, &ThisClass::select_shape) +
                  SSegmentedControl<ESbxMeshShape>::Slot(ESbxMeshShape::Box)
                      .Text(NSLOCTEXT("SbxMeshGenLab", "BoxShape", "Box")) +
                  SSegmentedControl<ESbxMeshShape>::Slot(ESbxMeshShape::Cylinder)
                      .Text(NSLOCTEXT("SbxMeshGenLab", "CylinderShape", "Cylinder")) +
                  SSegmentedControl<ESbxMeshShape>::Slot(ESbxMeshShape::Sphere)
                      .Text(NSLOCTEXT("SbxMeshGenLab", "SphereShape", "Sphere")) +
                  SSegmentedControl<ESbxMeshShape>::Slot(ESbxMeshShape::Cone)
                      .Text(NSLOCTEXT("SbxMeshGenLab", "ConeShape", "Cone")) +
                  SSegmentedControl<ESbxMeshShape>::Slot(ESbxMeshShape::HexFrame)
                      .Text(NSLOCTEXT("SbxMeshGenLab", "HexFrameShape", "Hex Frame"))] +
             SVerticalBox::Slot().AutoHeight()[box_controls] +
             SVerticalBox::Slot().AutoHeight()[cylinder_controls] +
             SVerticalBox::Slot().AutoHeight()[sphere_controls] +
             SVerticalBox::Slot().AutoHeight()[cone_controls] +
             SVerticalBox::Slot().AutoHeight()[hex_frame_controls] +
             SVerticalBox::Slot().AutoHeight()
                 [SNew(SButton)
                      .Text_Lambda([this] { return generate_button_text(); })
                      .ToolTipText(NSLOCTEXT("SbxMeshGenLab",
                                             "GenerateTooltip",
                                             "Create or replace the selected shape in the plugin's "
                                             "generated-content directory."))
                      .OnClicked_UObject(this, &ThisClass::generate_selected_shape)] +
             SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
                 [SAssignNew(status_text_, STextBlock)
                      .Text(
                          NSLOCTEXT("SbxMeshGenLab", "InitialStatus", "No mesh generated yet."))] +
             SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
                 [SNew(SBox)
                      .WidthOverride(static_cast<float>(thumbnail_size))
                      .HeightOverride(static_cast<float>(thumbnail_size))[preview_widget]]];
}

void USbxMeshGenLabWidget::ReleaseSlateResources(bool const release_children) {
    Super::ReleaseSlateResources(release_children);
    status_text_.Reset();
    thumbnail_.Reset();
    thumbnail_pool_.Reset();
}

auto USbxMeshGenLabWidget::generate_selected_shape() -> FReply {
    FSbxMeshData mesh_data{};
    switch (selected_shape_) {
        case ESbxMeshShape::Box:
            mesh_data = SandboxMesh::generate_box(FSbxBoxParameters{box_dimensions_});
            break;
        case ESbxMeshShape::Cylinder:
            mesh_data = SandboxMesh::generate_cylinder(FSbxCylinderParameters{
                cylinder_radius_, cylinder_height_, cylinder_radial_segments_});
            break;
        case ESbxMeshShape::Sphere:
            mesh_data = SandboxMesh::generate_sphere(FSbxSphereParameters{
                sphere_radius_, sphere_longitude_segments_, sphere_latitude_segments_});
            break;
        case ESbxMeshShape::Cone:
            mesh_data = SandboxMesh::generate_cone(
                FSbxConeParameters{cone_radius_, cone_height_, cone_radial_segments_});
            break;
        case ESbxMeshShape::HexFrame:
            if (hex_frame_wall_thickness_ >= hex_frame_outer_radius_) {
                status_text_->SetText(NSLOCTEXT("SbxMeshGenLab",
                                                "InvalidHexFrameThickness",
                                                "Wall thickness must be less than outer radius."));
                return FReply::Handled();
            }
            mesh_data =
                SandboxMesh::generate_hex_frame(FSbxHexFrameParameters{hex_frame_outer_radius_,
                                                                       hex_frame_wall_thickness_,
                                                                       hex_frame_depth_,
                                                                       hex_frame_pointy_top_});
            break;
    }

    set_preview_mesh(nullptr);
    auto* const static_mesh{
        SandboxMesh::write_generated_static_mesh_asset(mesh_data, generated_asset_name())};
    if (static_mesh == nullptr) {
        status_text_->SetText(NSLOCTEXT(
            "SbxMeshGenLab", "GenerationFailed", "Mesh generation failed; see Output Log."));
        return FReply::Handled();
    }

    status_text_->SetText(
        FText::Format(NSLOCTEXT("SbxMeshGenLab", "GenerationSucceeded", "Generated {0}."),
                      FText::FromString(static_mesh->GetPathName())));
    set_preview_mesh(static_mesh);
    return FReply::Handled();
}

void USbxMeshGenLabWidget::select_shape(ESbxMeshShape const shape) {
    selected_shape_ = shape;
    auto const generated_object_path{
        SandboxMesh::get_generated_asset_object_path(generated_asset_name())};
    set_preview_mesh(LoadObject<UStaticMesh>(nullptr, *generated_object_path));

    if (status_text_.IsValid()) {
        status_text_->SetText(FText::Format(
            NSLOCTEXT("SbxMeshGenLab", "SelectedShape", "Selected {0}."), selected_shape_text()));
    }
}

auto USbxMeshGenLabWidget::generated_asset_name() const -> FName {
    switch (selected_shape_) {
        case ESbxMeshShape::Box:
            return generated_box_asset_name;
        case ESbxMeshShape::Cylinder:
            return generated_cylinder_asset_name;
        case ESbxMeshShape::Sphere:
            return generated_sphere_asset_name;
        case ESbxMeshShape::Cone:
            return generated_cone_asset_name;
        case ESbxMeshShape::HexFrame:
            return generated_hex_frame_asset_name;
    }

    checkNoEntry();
    return NAME_None;
}

auto USbxMeshGenLabWidget::selected_shape_text() const -> FText {
    switch (selected_shape_) {
        case ESbxMeshShape::Box:
            return NSLOCTEXT("SbxMeshGenLab", "Box", "Box");
        case ESbxMeshShape::Cylinder:
            return NSLOCTEXT("SbxMeshGenLab", "Cylinder", "Cylinder");
        case ESbxMeshShape::Sphere:
            return NSLOCTEXT("SbxMeshGenLab", "Sphere", "Sphere");
        case ESbxMeshShape::Cone:
            return NSLOCTEXT("SbxMeshGenLab", "Cone", "Cone");
        case ESbxMeshShape::HexFrame:
            return NSLOCTEXT("SbxMeshGenLab", "HexFrame", "Hex Frame");
    }

    checkNoEntry();
    return FText::GetEmpty();
}

auto USbxMeshGenLabWidget::generate_button_text() const -> FText {
    switch (selected_shape_) {
        case ESbxMeshShape::Box:
            return NSLOCTEXT("SbxMeshGenLab", "GenerateBox", "Generate Box");
        case ESbxMeshShape::Cylinder:
            return NSLOCTEXT("SbxMeshGenLab", "GenerateCylinder", "Generate Cylinder");
        case ESbxMeshShape::Sphere:
            return NSLOCTEXT("SbxMeshGenLab", "GenerateSphere", "Generate Sphere");
        case ESbxMeshShape::Cone:
            return NSLOCTEXT("SbxMeshGenLab", "GenerateCone", "Generate Cone");
        case ESbxMeshShape::HexFrame:
            return NSLOCTEXT("SbxMeshGenLab", "GenerateHexFrame", "Generate Hex Frame");
    }

    checkNoEntry();
    return FText::GetEmpty();
}

void USbxMeshGenLabWidget::set_preview_mesh(UStaticMesh* const static_mesh) {
    if (!thumbnail_.IsValid()) {
        return;
    }

    if (static_mesh == nullptr) {
        thumbnail_->SetAsset(FAssetData{});
        return;
    }

    thumbnail_->SetAsset(static_mesh);
    thumbnail_->RefreshThumbnail();
}
