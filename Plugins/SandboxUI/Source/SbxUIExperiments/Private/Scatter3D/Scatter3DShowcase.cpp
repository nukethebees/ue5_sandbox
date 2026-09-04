#include "SbxUIExperiments/Scatter3D/Scatter3DShowcase.h"

#include "Benchmarks/Scatter3D/Scatter3DBenchmark.h"
#include "SandboxUI/slate/SlateSlots.h"
#include "SandboxUI/widgets/SLabeledRow.h"
#include "SScatter3DWidget.h"
#include "Widgets/SExperimentPanel.h"

#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

UScatter3DShowcase::UScatter3DShowcase() {
    TabDisplayName = NSLOCTEXT("Scatter3D", "ShowcaseTabName", "RDG 3D Scatter Experiment");
    bAlwaysReregisterWithWindowsMenu = true;
}

TSharedRef<SWidget> UScatter3DShowcase::RebuildWidget() {
    auto const scatter_widget{SNew(SScatter3DWidget)};

    return SNew(SExperimentPanel)
        .Title(NSLOCTEXT("Scatter3D", "Title", "RDG 3D Scatter Experiment"))
        .Description(NSLOCTEXT("Scatter3D",
                               "Description",
                               "Deterministic CPU clusters are uploaded only when changed, "
                               "rasterized by RDG, and displayed as one Slate image."))
        .Controls()
            [SNew(SVerticalBox) +
             SandboxUI::Slate::vbox_auto_slot(FMargin{0.0f, 0.0f, 0.0f, 10.0f})
                 [SNew(SLabeledRow)
                      .Label(NSLOCTEXT("Scatter3D", "PointCount", "Points"))
                      .ContentHAlign(HAlign_Left)[SNew(SSpinBox<int32>)
                                                      .MinValue(1)
                                                      .MaxValue(65536)
                                                      .MinSliderValue(1)
                                                      .MaxSliderValue(65536)
                                                      .SliderExponent(4.0f)
                                                      .Delta(1)
                                                      .MinDesiredWidth(140.0f)
                                                      .Value(4096)
                                                      .OnValueCommitted_Lambda(
                                                          [scatter_widget](int32 const point_count,
                                                                           ETextCommit::Type) {
                                                              scatter_widget->set_point_count(
                                                                  point_count);
                                                          })]] +
             SandboxUI::Slate::vbox_auto_slot()
                 [SNew(SExperimentBenchmark)
                      .ButtonText(
                          NSLOCTEXT("Scatter3D", "RunBenchmark", "Benchmark RDG point scaling"))
                      .ToolTipText(
                          NSLOCTEXT("Scatter3D",
                                    "RunBenchmarkTooltip",
                                    "Runs a short 512x512 benchmark from 1 to 65,536 points."))
                      .OnClicked_UObject(this, &UScatter3DShowcase::run_benchmark)
                      .Output()[SAssignNew(benchmark_output_, SMultiLineEditableTextBox)
                                    .IsReadOnly(true)
                                    .Text(NSLOCTEXT("Scatter3D",
                                                    "BenchmarkInstructions",
                                                    "Results appear here. The CLI writes the same "
                                                    "stages to CSV."))]]]
        .Preview()[SNew(SBox).WidthOverride(512.0f).HeightOverride(512.0f)[scatter_widget]];
}

auto UScatter3DShowcase::run_benchmark() -> FReply {
    FScatter3DBenchmarkOptions options;
    options.warmup_iterations = 2;
    options.measured_iterations = 10;
    auto const report{run_scatter_3d_benchmark(options)};
    benchmark_output_->SetText(FText::FromString(report.to_text()));
    return FReply::Handled();
}
