#include "SbxUIExperiments/Scatter3D/Scatter3DShowcase.h"

#include <utility>

#include "Benchmarks/Scatter3D/Scatter3DBenchmark.h"
#include "SandboxUI/slate/SlateSlots.h"
#include "SandboxUI/widgets/SLabeledRow.h"
#include "SScatter3DWidget.h"
#include "Widgets/SExperimentPanel.h"

#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

#include "generated/UScatter3DShowcase.slate.generated.h"

UScatter3DShowcase::UScatter3DShowcase() {
    TabDisplayName = NSLOCTEXT("Scatter3D", "ShowcaseTabName", "RDG 3D Scatter Experiment");
    bAlwaysReregisterWithWindowsMenu = true;
}

TSharedRef<SWidget> UScatter3DShowcase::RebuildWidget() {
    auto const scatter_widget{SNew(SScatter3DWidget)};
    auto on_value_committed{[scatter_widget](int32 const point_count, ETextCommit::Type) {
        scatter_widget->set_point_count(point_count);
    }};

    return SlateGenerated::UScatter3DShowcaseBuilder{*this}.RebuildWidget(
        std::move(on_value_committed), scatter_widget);
}

auto UScatter3DShowcase::run_benchmark() -> FReply {
    FScatter3DBenchmarkOptions options;
    options.warmup_iterations = 2;
    options.measured_iterations = 10;
    auto const report{run_scatter_3d_benchmark(options)};
    benchmark_output_->SetText(FText::FromString(report.to_text()));
    return FReply::Handled();
}
