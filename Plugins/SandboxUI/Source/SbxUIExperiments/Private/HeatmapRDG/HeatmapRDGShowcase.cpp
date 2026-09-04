#include "SbxUIExperiments/HeatmapRDG/HeatmapRDGShowcase.h"

#include "Benchmarks/Heatmap/HeatmapBenchmark.h"
#include "SandboxUI/slate/SlateSlots.h"
#include "SandboxUI/widgets/SLabeledRow.h"
#include "SbxUIExperiments/HeatmapRDG/HeatmapRDGWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SExperimentPanel.h"

#include "generated/UHeatmapRDGShowcase.slate.generated.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeatmapRDGShowcase, Log, All);

UHeatmapRDGShowcase::UHeatmapRDGShowcase() {
    TabDisplayName = NSLOCTEXT("HeatmapRDG", "ShowcaseTabName", "RDG Heatmap Showcase");
    bAlwaysReregisterWithWindowsMenu = true;
}

TSharedRef<SWidget> UHeatmapRDGShowcase::RebuildWidget() {
    heatmap_widget_ = NewObject<UHeatmapRDGWidget>(this);
    check(heatmap_widget_);

    auto builder{SlateGenerated::UHeatmapRDGShowcaseBuilder{*this}};
    auto make_size_button{[&builder](FText const& label, FOnClicked on_clicked) {
        return builder.BuildSizeButton(label, MoveTemp(on_clicked));
    }};
    auto const size_32_button{make_size_button(
        FText::FromString(TEXT("32 x 32")),
        FOnClicked::CreateUObject(this, &UHeatmapRDGShowcase::select_grid_size, 32))};
    auto const size_64_button{make_size_button(
        FText::FromString(TEXT("64 x 64")),
        FOnClicked::CreateUObject(this, &UHeatmapRDGShowcase::select_grid_size, 64))};
    auto const size_128_button{make_size_button(
        FText::FromString(TEXT("128 x 128")),
        FOnClicked::CreateUObject(this, &UHeatmapRDGShowcase::select_grid_size, 128))};
    auto const size_256_button{make_size_button(
        FText::FromString(TEXT("256 x 256")),
        FOnClicked::CreateUObject(this, &UHeatmapRDGShowcase::select_grid_size, 256))};
    auto const size_512_button{make_size_button(
        FText::FromString(TEXT("512 x 512")),
        FOnClicked::CreateUObject(this, &UHeatmapRDGShowcase::select_grid_size, 512))};
    auto const heatmap_preview{heatmap_widget_->TakeWidget()};

    return builder.RebuildWidget(size_32_button,
                                 size_64_button,
                                 size_128_button,
                                 size_256_button,
                                 size_512_button,
                                 heatmap_preview);
}

auto UHeatmapRDGShowcase::select_grid_size(int32 const grid_size) -> FReply {
    grid_size_ = grid_size;
    regenerate_selected_pattern();
    return FReply::Handled();
}

auto UHeatmapRDGShowcase::show_hotspots() -> FReply {
    selected_pattern_ = EPattern::Hotspots;
    regenerate_selected_pattern();
    return FReply::Handled();
}

auto UHeatmapRDGShowcase::show_gradient() -> FReply {
    selected_pattern_ = EPattern::GradientChecker;
    regenerate_selected_pattern();
    return FReply::Handled();
}

auto UHeatmapRDGShowcase::run_benchmark() -> FReply {
    FHeatmapBenchmarkOptions options;
    options.warmup_iterations = 2;
    options.measured_iterations = 10;
    auto const report{run_heatmap_benchmark(options)};
    benchmark_output_->SetText(FText::FromString(report.to_text()));
    return FReply::Handled();
}

void UHeatmapRDGShowcase::regenerate_selected_pattern() {
    if (selected_pattern_ == EPattern::Hotspots) {
        heatmap_widget_->generate_demo_grid(grid_size_, grid_size_);
        return;
    }

    generate_gradient_grid();
}

void UHeatmapRDGShowcase::generate_gradient_grid() {
    FHeatmapRDGGrid grid;
    grid.width = grid_size_;
    grid.height = grid_size_;
    grid.values.SetNumUninitialized(grid.width * grid.height);

    auto const width_denominator{static_cast<float>(grid.width - 1)};
    auto const height_denominator{static_cast<float>(grid.height - 1)};
    for (int32 y{0}; y < grid.height; ++y) {
        for (int32 x{0}; x < grid.width; ++x) {
            auto const gradient{(static_cast<float>(x) / width_denominator) *
                                (static_cast<float>(y) / height_denominator)};
            auto const checker{((x / 8) + (y / 8)) % 2 == 0 ? 0.18f : 0.0f};
            grid.values[y * grid.width + x] = FMath::Clamp(gradient + checker, 0.0f, 1.0f);
        }
    }

    if (!heatmap_widget_->set_grid(MoveTemp(grid))) {
        UE_LOG(LogHeatmapRDGShowcase, Warning, TEXT("Failed to submit the gradient grid."));
    }
}
