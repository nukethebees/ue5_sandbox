#include "SbxUIExperiments/VolumeHeatmap3D/VolumeHeatmap3DShowcase.h"

#include "Benchmarks/VolumeHeatmap3D/VolumeHeatmap3DBenchmark.h"
#include "SandboxUI/slate/SlateSlots.h"
#include "SandboxUI/widgets/SLabeledRow.h"
#include "SVolumeHeatmap3DWidget.h"
#include "Widgets/SExperimentPanel.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

UVolumeHeatmap3DShowcase::UVolumeHeatmap3DShowcase() {
    TabDisplayName = NSLOCTEXT("VolumeHeatmap3D", "ShowcaseTabName", "RDG 3D Volume Heatmap");
    bAlwaysReregisterWithWindowsMenu = true;
}

TSharedRef<SWidget> UVolumeHeatmap3DShowcase::RebuildWidget() {
    constexpr auto label_min_width{56.0f};
    auto const volume_widget{SNew(SVolumeHeatmap3DWidget)};
    auto grid_button = [volume_widget](int32 const dimension) -> TSharedRef<SWidget> {
        return SNew(SButton)
            .Text(FText::FromString(FString::Printf(TEXT("%d³"), dimension)))
            .OnClicked_Lambda([volume_widget, dimension] {
                volume_widget->set_grid_dimension(dimension);
                return FReply::Handled();
            });
    };

    return SNew(SExperimentPanel)
        .Title(NSLOCTEXT("VolumeHeatmap3D", "Title", "RDG 3D Volume Heatmap"))
        .Description(NSLOCTEXT("VolumeHeatmap3D",
                               "Description",
                               "A dense CPU volume is uploaded on change and composited as one "
                               "instanced, view-aligned slice draw."))
        .Controls()
            [SNew(SVerticalBox) +
             SandboxUI::Slate::vbox_auto_slot(FMargin{0.0f, 0.0f, 0.0f, 5.0f})
                 [SNew(SHorizontalBox) +
                  SandboxUI::Slate::hbox_auto_slot(FMargin{0.0f, 0.0f, 6.0f, 0.0f})
                      [SNew(SButton)
                           .Text(NSLOCTEXT("VolumeHeatmap3D", "Clouds", "Gaussian clouds"))
                           .OnClicked_Lambda([volume_widget] {
                               volume_widget->set_pattern(EVolumeHeatmap3DPattern::GaussianClouds);
                               return FReply::Handled();
                           })] +
                  SandboxUI::Slate::hbox_auto_slot()
                      [SNew(SButton)
                           .Text(NSLOCTEXT("VolumeHeatmap3D", "Shell", "Hollow shell"))
                           .OnClicked_Lambda([volume_widget] {
                               volume_widget->set_pattern(EVolumeHeatmap3DPattern::HollowShell);
                               return FReply::Handled();
                           })]] +
             SandboxUI::Slate::vbox_auto_slot(FMargin{0.0f, 0.0f, 0.0f, 5.0f})
                 [SNew(SLabeledRow)
                      .Label(NSLOCTEXT("VolumeHeatmap3D", "Grid", "Grid"))
                      .LabelMinWidth(
                          label_min_width)[SNew(SHorizontalBox) +
                                           SandboxUI::Slate::hbox_auto_slot(
                                               FMargin{0.0f, 0.0f, 3.0f, 0.0f})[grid_button(16)] +
                                           SandboxUI::Slate::hbox_auto_slot(
                                               FMargin{0.0f, 0.0f, 3.0f, 0.0f})[grid_button(32)] +
                                           SandboxUI::Slate::hbox_auto_slot(
                                               FMargin{0.0f, 0.0f, 3.0f, 0.0f})[grid_button(64)] +
                                           SandboxUI::Slate::hbox_auto_slot()[grid_button(128)]]] +
             SandboxUI::Slate::vbox_auto_slot(FMargin{0.0f, 0.0f, 0.0f, 5.0f})
                 [SNew(SHorizontalBox) +
                  SandboxUI::Slate::hbox_auto_slot()
                      [SNew(SLabeledRow)
                           .Label(NSLOCTEXT("VolumeHeatmap3D", "Slices", "Slices"))
                           .LabelMinWidth(label_min_width)
                           .ContentHAlign(HAlign_Left)[SNew(SSpinBox<int32>)
                                                           .MinValue(8)
                                                           .MaxValue(256)
                                                           .MinSliderValue(8)
                                                           .MaxSliderValue(256)
                                                           .Delta(8)
                                                           .MinDesiredWidth(100)
                                                           .Value(96)
                                                           .OnValueCommitted_Lambda(
                                                               [volume_widget](int32 const value,
                                                                               ETextCommit::Type) {
                                                                   volume_widget->set_slice_count(
                                                                       value);
                                                               })]] +
                  SandboxUI::Slate::hbox_auto_slot(FMargin{14.0f, 0.0f, 0.0f, 0.0f})
                      [SNew(SLabeledRow)
                           .Label(NSLOCTEXT("VolumeHeatmap3D", "Density", "Density"))
                           .LabelMinWidth(label_min_width)
                           .ContentHAlign(HAlign_Left)[SNew(SSpinBox<float>)
                                                           .MinValue(0.25f)
                                                           .MaxValue(8.0f)
                                                           .MinSliderValue(0.25f)
                                                           .MaxSliderValue(8.0f)
                                                           .Delta(0.25f)
                                                           .MinDesiredWidth(100)
                                                           .Value(3.0f)
                                                           .OnValueCommitted_Lambda(
                                                               [volume_widget](float const value,
                                                                               ETextCommit::Type) {
                                                                   volume_widget->set_density_scale(
                                                                       value);
                                                               })]]] +
             SandboxUI::Slate::vbox_auto_slot(FMargin{0.0f, 0.0f, 0.0f, 8.0f})
                 [SNew(SHorizontalBox) +
                  SandboxUI::Slate::hbox_auto_slot()
                      [SNew(SLabeledRow)
                           .Label(NSLOCTEXT("VolumeHeatmap3D", "Yaw", "Yaw"))
                           .LabelMinWidth(label_min_width)
                           .ContentHAlign(HAlign_Left)[SNew(SSpinBox<float>)
                                                           .MinValue(-180)
                                                           .MaxValue(180)
                                                           .MinSliderValue(-180)
                                                           .MaxSliderValue(180)
                                                           .Delta(1)
                                                           .MinDesiredWidth(90)
                                                           .Value(-51)
                                                           .OnValueCommitted_Lambda(
                                                               [volume_widget](float const value,
                                                                               ETextCommit::Type) {
                                                                   volume_widget->set_yaw(value);
                                                               })]] +
                  SandboxUI::Slate::hbox_auto_slot(FMargin{14.0f, 0.0f, 0.0f, 0.0f})
                      [SNew(SLabeledRow)
                           .Label(NSLOCTEXT("VolumeHeatmap3D", "Pitch", "Pitch"))
                           .LabelMinWidth(label_min_width)
                           .ContentHAlign(HAlign_Left)[SNew(SSpinBox<float>)
                                                           .MinValue(-85)
                                                           .MaxValue(85)
                                                           .MinSliderValue(-85)
                                                           .MaxSliderValue(85)
                                                           .Delta(1)
                                                           .MinDesiredWidth(90)
                                                           .Value(29)
                                                           .OnValueCommitted_Lambda(
                                                               [volume_widget](float const value,
                                                                               ETextCommit::Type) {
                                                                   volume_widget->set_pitch(value);
                                                               })]]] +
             SandboxUI::Slate::vbox_auto_slot()
                 [SNew(SExperimentBenchmark)
                      .ButtonText(NSLOCTEXT(
                          "VolumeHeatmap3D", "RunBenchmark", "Benchmark grid and slice scaling"))
                      .OnClicked_UObject(this, &UVolumeHeatmap3DShowcase::run_benchmark)
                      .OutputHeight(105.0f)
                      .Output()[SAssignNew(benchmark_output_, SMultiLineEditableTextBox)
                                    .IsReadOnly(true)
                                    .Text(NSLOCTEXT("VolumeHeatmap3D",
                                                    "BenchmarkInstructions",
                                                    "Results appear here. The CLI writes the same "
                                                    "stages to CSV."))]]]
        .Preview()[SNew(SBox).WidthOverride(512.0f).HeightOverride(512.0f)[volume_widget]];
}

auto UVolumeHeatmap3DShowcase::run_benchmark() -> FReply {
    FVolumeHeatmap3DBenchmarkOptions options;
    options.warmup_iterations = 2;
    options.measured_iterations = 10;
    auto const report{run_volume_heatmap_3d_benchmark(options)};
    benchmark_output_->SetText(FText::FromString(report.to_text()));
    return FReply::Handled();
}
