#include "SbxUIExperiments/Radar3D/Radar3DShowcase.h"

#include <utility>

#include "Benchmarks/Radar3D/Radar3DBenchmark.h"
#include "SandboxUI/slate/SlateSlots.h"
#include "SandboxUI/widgets/SLabeledRow.h"
#include "SRadar3DWidget.h"
#include "Widgets/SExperimentPanel.h"

#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

#include "generated/URadar3DShowcase.slate.generated.h"

URadar3DShowcase::URadar3DShowcase() {
    TabDisplayName = NSLOCTEXT("Radar3D", "ShowcaseTabName", "RDG 3D Radar Experiment");
    bAlwaysReregisterWithWindowsMenu = true;
}

TSharedRef<SWidget> URadar3DShowcase::RebuildWidget() {
    auto builder{SlateGenerated::URadar3DShowcaseBuilder{*this}};
    auto const radar_widget{builder.BuildRadarWidget()};
    auto on_value_changed{[radar_widget](int32 const contact_count) {
        radar_widget->set_contact_count(contact_count);
    }};

    return builder.RebuildWidget(std::move(on_value_changed), radar_widget);
}

auto URadar3DShowcase::run_benchmark() -> FReply {
    FRadar3DBenchmarkOptions options;
    options.warmup_iterations = 2;
    options.measured_iterations = 10;
    auto const report{run_radar_3d_benchmark(options)};
    benchmark_output_->SetText(FText::FromString(report.to_text()));
    return FReply::Handled();
}
