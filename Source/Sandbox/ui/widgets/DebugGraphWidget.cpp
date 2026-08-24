#include "Sandbox/ui/widgets/DebugGraphWidget.h"

#include "SandboxUI/widgets/SGraphPlot.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

void UDebugGraphWidget::set_samples(TConstArrayView<FVector2d> const in_samples,
                                    int32 const new_oldest_index) {
    auto const sample_count{in_samples.Num()};
    x_.Reset(sample_count);
    y_.Reset(sample_count);

    if (sample_count > 0) {
        check(new_oldest_index >= 0 && new_oldest_index < sample_count);
        auto const oldest_index{new_oldest_index};
        auto const first_x{in_samples[oldest_index].X};
        for (int32 i = 0; i < sample_count; ++i) {
            auto const index{(oldest_index + i) % sample_count};
            auto const& sample{in_samples[index]};
            x_.Add(static_cast<float>(sample.X - first_x));
            y_.Add(static_cast<float>(sample.Y));
        }
    }

    update_slate_series();
}

TSharedRef<SWidget> UDebugGraphWidget::RebuildWidget() {
    SAssignNew(graph_widget_, SGraphPlot);
    update_slate_series();
    return graph_widget_.ToSharedRef();
}

void UDebugGraphWidget::ReleaseSlateResources(bool const release_children) {
    Super::ReleaseSlateResources(release_children);
    graph_widget_.Reset();
}

void UDebugGraphWidget::update_slate_series() {
    if (!graph_widget_.IsValid()) {
        return;
    }

    static FText const speed_series_name{FText::FromString(TEXT("Speed"))};
    FGraphSeries series{
        .name = speed_series_name,
        .x = x_,
        .y = y_,
        .style = {.color = FLinearColor::Green, .thickness = 1.0f, .antialias = false},
    };
    TArray<FGraphSeries> series_snapshot;
    series_snapshot.Add(MoveTemp(series));
    graph_widget_->set_series(MoveTemp(series_snapshot));
}
