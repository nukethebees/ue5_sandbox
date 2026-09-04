#include "SbxUIExperiments/VolumeHeatmap3D/VolumeHeatmap3DShowcase.h"

#include <utility>

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

#include "generated/UVolumeHeatmap3DShowcase.slate.generated.h"

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
    auto show_clouds = [volume_widget] {
        volume_widget->set_pattern(EVolumeHeatmap3DPattern::GaussianClouds);
        return FReply::Handled();
    };
    auto show_shell = [volume_widget] {
        volume_widget->set_pattern(EVolumeHeatmap3DPattern::HollowShell);
        return FReply::Handled();
    };
    auto set_slice_count = [volume_widget](int32 const value, ETextCommit::Type) {
        volume_widget->set_slice_count(value);
    };
    auto set_density_scale = [volume_widget](float const value, ETextCommit::Type) {
        volume_widget->set_density_scale(value);
    };
    auto set_yaw = [volume_widget](float const value, ETextCommit::Type) {
        volume_widget->set_yaw(value);
    };
    auto set_pitch = [volume_widget](float const value, ETextCommit::Type) {
        volume_widget->set_pitch(value);
    };

    return SlateGenerated::UVolumeHeatmap3DShowcaseBuilder{*this}.RebuildWidget(
        std::move(show_clouds),
        std::move(show_shell),
        grid_button,
        std::move(set_slice_count),
        std::move(set_density_scale),
        std::move(set_yaw),
        std::move(set_pitch),
        volume_widget);
}

auto UVolumeHeatmap3DShowcase::run_benchmark() -> FReply {
    FVolumeHeatmap3DBenchmarkOptions options;
    options.warmup_iterations = 2;
    options.measured_iterations = 10;
    auto const report{run_volume_heatmap_3d_benchmark(options)};
    benchmark_output_->SetText(FText::FromString(report.to_text()));
    return FReply::Handled();
}
