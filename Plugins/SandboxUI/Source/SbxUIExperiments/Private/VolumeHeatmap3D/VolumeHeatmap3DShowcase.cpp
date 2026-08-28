#include "SbxUIExperiments/VolumeHeatmap3D/VolumeHeatmap3DShowcase.h"

#include "Benchmarks/VolumeHeatmap3D/VolumeHeatmap3DBenchmark.h"
#include "SVolumeHeatmap3DWidget.h"

#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

UVolumeHeatmap3DShowcase::UVolumeHeatmap3DShowcase() {
    TabDisplayName = NSLOCTEXT("VolumeHeatmap3D", "ShowcaseTabName", "RDG 3D Volume Heatmap");
    bAlwaysReregisterWithWindowsMenu = true;
}

TSharedRef<SWidget> UVolumeHeatmap3DShowcase::RebuildWidget() {
    auto const volume_widget{SNew(SVolumeHeatmap3DWidget)};
    auto grid_button = [volume_widget](int32 const dimension) -> TSharedRef<SWidget> {
        return SNew(SButton)
            .Text(FText::FromString(FString::Printf(TEXT("%d³"), dimension)))
            .OnClicked_Lambda([volume_widget, dimension] {
                volume_widget->set_grid_dimension(dimension);
                return FReply::Handled();
            });
    };

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(12.0f)
            [SNew(SVerticalBox) +
             SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                 [SNew(STextBlock)
                      .Text(NSLOCTEXT("VolumeHeatmap3D", "Title", "RDG 3D Volume Heatmap"))
                      .Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))] +
             SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                 [SNew(STextBlock)
                      .Text(NSLOCTEXT(
                          "VolumeHeatmap3D",
                          "Description",
                          "A dense CPU volume is uploaded on change and composited as one "
                          "instanced, view-aligned slice draw."))] +
             SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
                 [SNew(SHorizontalBox) +
                  SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
                      [SNew(SButton)
                           .Text(NSLOCTEXT("VolumeHeatmap3D", "Clouds", "Gaussian clouds"))
                           .OnClicked_Lambda([volume_widget] {
                               volume_widget->set_pattern(EVolumeHeatmap3DPattern::GaussianClouds);
                               return FReply::Handled();
                           })] +
                  SHorizontalBox::Slot()
                      .AutoWidth()[SNew(SButton)
                                       .Text(NSLOCTEXT("VolumeHeatmap3D", "Shell", "Hollow shell"))
                                       .OnClicked_Lambda([volume_widget] {
                                           volume_widget->set_pattern(
                                               EVolumeHeatmap3DPattern::HollowShell);
                                           return FReply::Handled();
                                       })]] +
             SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
                 [SNew(SHorizontalBox) +
                  SHorizontalBox::Slot()
                      .AutoWidth()
                      .VAlign(VAlign_Center)
                      .Padding(0, 0, 8, 0)
                          [SNew(STextBlock).Text(NSLOCTEXT("VolumeHeatmap3D", "Grid", "Grid"))] +
                  SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 3, 0)[grid_button(16)] +
                  SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 3, 0)[grid_button(32)] +
                  SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 3, 0)[grid_button(64)] +
                  SHorizontalBox::Slot().AutoWidth()[grid_button(128)]] +
             SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
                 [SNew(SHorizontalBox) +
                  SHorizontalBox::Slot()
                      .AutoWidth()
                      .VAlign(VAlign_Center)
                      .Padding(
                          0, 0, 8, 0)[SNew(STextBlock)
                                          .Text(NSLOCTEXT("VolumeHeatmap3D", "Slices", "Slices"))] +
                  SHorizontalBox::Slot()
                      .AutoWidth()[SNew(SSpinBox<int32>)
                                       .MinValue(8)
                                       .MaxValue(256)
                                       .MinSliderValue(8)
                                       .MaxSliderValue(256)
                                       .Delta(8)
                                       .MinDesiredWidth(100)
                                       .Value(96)
                                       .OnValueCommitted_Lambda(
                                           [volume_widget](int32 const value, ETextCommit::Type) {
                                               volume_widget->set_slice_count(value);
                                           })] +
                  SHorizontalBox::Slot()
                      .AutoWidth()
                      .VAlign(VAlign_Center)
                      .Padding(14, 0, 8, 0)[SNew(STextBlock)
                                                .Text(NSLOCTEXT(
                                                    "VolumeHeatmap3D", "Density", "Density"))] +
                  SHorizontalBox::Slot()
                      .AutoWidth()[SNew(SSpinBox<float>)
                                       .MinValue(0.25f)
                                       .MaxValue(8.0f)
                                       .MinSliderValue(0.25f)
                                       .MaxSliderValue(8.0f)
                                       .Delta(0.25f)
                                       .MinDesiredWidth(100)
                                       .Value(3.0f)
                                       .OnValueCommitted_Lambda(
                                           [volume_widget](float const value, ETextCommit::Type) {
                                               volume_widget->set_density_scale(value);
                                           })]] +
             SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                 [SNew(SHorizontalBox) +
                  SHorizontalBox::Slot()
                      .AutoWidth()
                      .VAlign(VAlign_Center)
                      .Padding(0, 0, 8, 0)[SNew(STextBlock)
                                               .Text(NSLOCTEXT("VolumeHeatmap3D", "Yaw", "Yaw"))] +
                  SHorizontalBox::Slot()
                      .AutoWidth()[SNew(SSpinBox<float>)
                                       .MinValue(-180)
                                       .MaxValue(180)
                                       .MinSliderValue(-180)
                                       .MaxSliderValue(180)
                                       .Delta(1)
                                       .MinDesiredWidth(90)
                                       .Value(-51)
                                       .OnValueCommitted_Lambda(
                                           [volume_widget](float const value, ETextCommit::Type) {
                                               volume_widget->set_yaw(value);
                                           })] +
                  SHorizontalBox::Slot()
                      .AutoWidth()
                      .VAlign(VAlign_Center)
                      .Padding(14, 0, 8, 0)
                          [SNew(STextBlock).Text(NSLOCTEXT("VolumeHeatmap3D", "Pitch", "Pitch"))] +
                  SHorizontalBox::Slot()
                      .AutoWidth()[SNew(SSpinBox<float>)
                                       .MinValue(-85)
                                       .MaxValue(85)
                                       .MinSliderValue(-85)
                                       .MaxSliderValue(85)
                                       .Delta(1)
                                       .MinDesiredWidth(90)
                                       .Value(29)
                                       .OnValueCommitted_Lambda(
                                           [volume_widget](float const value, ETextCommit::Type) {
                                               volume_widget->set_pitch(value);
                                           })]] +
             SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                 [SNew(SVerticalBox) +
                  SVerticalBox::Slot().AutoHeight()
                      [SNew(SButton)
                           .Text(NSLOCTEXT("VolumeHeatmap3D",
                                           "RunBenchmark",
                                           "Benchmark grid and slice scaling"))
                           .OnClicked_UObject(this, &UVolumeHeatmap3DShowcase::run_benchmark)] +
                  SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)[SNew(SBox).HeightOverride(
                      105)[SAssignNew(benchmark_output_, SMultiLineEditableTextBox)
                               .IsReadOnly(true)
                               .Text(NSLOCTEXT("VolumeHeatmap3D",
                                               "BenchmarkInstructions",
                                               "Results appear here. The CLI writes the same "
                                               "stages to CSV."))]]] +
             SVerticalBox::Slot()
                 .FillHeight(1)
                 .HAlign(HAlign_Center)
                 .VAlign(VAlign_Center)[SNew(SBox).WidthOverride(512).HeightOverride(
                     512)[volume_widget]]];
}

auto UVolumeHeatmap3DShowcase::run_benchmark() -> FReply {
    FVolumeHeatmap3DBenchmarkOptions options;
    options.warmup_iterations = 2;
    options.measured_iterations = 10;
    auto const report{run_volume_heatmap_3d_benchmark(options)};
    benchmark_output_->SetText(FText::FromString(report.to_text()));
    return FReply::Handled();
}
