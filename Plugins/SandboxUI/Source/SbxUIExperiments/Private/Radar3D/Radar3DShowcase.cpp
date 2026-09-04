#include "SbxUIExperiments/Radar3D/Radar3DShowcase.h"

#include "Benchmarks/Radar3D/Radar3DBenchmark.h"
#include "SandboxUI/slate/SlateSlots.h"
#include "SandboxUI/widgets/SLabeledRow.h"
#include "SRadar3DWidget.h"
#include "Widgets/SExperimentPanel.h"

#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

URadar3DShowcase::URadar3DShowcase() {
    TabDisplayName = NSLOCTEXT("Radar3D", "ShowcaseTabName", "RDG 3D Radar Experiment");
    bAlwaysReregisterWithWindowsMenu = true;
}

TSharedRef<SWidget> URadar3DShowcase::RebuildWidget() {
    auto const radar_widget{SNew(SRadar3DWidget)};

    return SNew(SExperimentPanel)
        .Title(NSLOCTEXT("Radar3D", "Title", "RDG 3D Radar Experiment"))
        .Description(NSLOCTEXT("Radar3D",
                               "Description",
                               "Synthetic CPU contacts are rendered by RDG into one offscreen "
                               "texture and displayed by Slate."))
        .Controls()
            [SNew(SVerticalBox) +
             SandboxUI::Slate::vbox_auto_slot(FMargin{0.0f, 0.0f, 0.0f, 10.0f})
                 [SNew(SLabeledRow)
                      .Label(NSLOCTEXT("Radar3D", "ContactCount", "Contacts"))
                      .ContentHAlign(HAlign_Left)
                          [SNew(SSpinBox<int32>)
                               .MinValue(1)
                               .MaxValue(256)
                               .MinSliderValue(1)
                               .MaxSliderValue(256)
                               .Delta(1)
                               .MinDesiredWidth(120.0f)
                               .Value(5)
                               .OnValueChanged_Lambda([radar_widget](int32 const contact_count) {
                                   radar_widget->set_contact_count(contact_count);
                               })]] +
             SandboxUI::Slate::vbox_auto_slot()
                 [SNew(SExperimentBenchmark)
                      .ButtonText(
                          NSLOCTEXT("Radar3D", "RunBenchmark", "Benchmark RDG contact scaling"))
                      .ToolTipText(
                          NSLOCTEXT("Radar3D",
                                    "RunBenchmarkTooltip",
                                    "Runs a short 512x512 benchmark from 1 to 256 contacts."))
                      .OnClicked_UObject(this, &URadar3DShowcase::run_benchmark)
                      .Output()[SAssignNew(benchmark_output_, SMultiLineEditableTextBox)
                                    .IsReadOnly(true)
                                    .Text(NSLOCTEXT(
                                        "Radar3D",
                                        "BenchmarkInstructions",
                                        "Results appear here. The CLI writes the same stages "
                                        "to CSV."))]]]
        .Preview()[SNew(SBox).WidthOverride(512.0f).HeightOverride(512.0f)[radar_widget]];
}

auto URadar3DShowcase::run_benchmark() -> FReply {
    FRadar3DBenchmarkOptions options;
    options.warmup_iterations = 2;
    options.measured_iterations = 10;
    auto const report{run_radar_3d_benchmark(options)};
    benchmark_output_->SetText(FText::FromString(report.to_text()));
    return FReply::Handled();
}
