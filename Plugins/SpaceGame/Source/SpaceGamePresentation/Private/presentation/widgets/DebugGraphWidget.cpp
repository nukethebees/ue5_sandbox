#include "SpaceGamePresentation/presentation/widgets/DebugGraphWidget.h"

#include "SpaceGamePresentation/ui/style/GameUiStyle.h"

#include "SandboxUI/widgets/SGraphPlot.h"

#include "Widgets/DeclarativeSyntaxSupport.h"

#include <utility>

void UDebugGraphWidget::set_samples(std::span<ioj::Vector2d const> const samples,
                                    std::uint32_t const oldest_index) {
    auto const sample_count{samples.size()};
    check(std::in_range<int32>(sample_count));
    auto const count{static_cast<int32>(sample_count)};
    auto changed{x_.Num() != count || y_.Num() != count};
    x_.SetNum(count);
    y_.SetNum(count);

    if (sample_count > 0) {
        check(oldest_index < sample_count);
        auto const first_x{samples[oldest_index].x()};
        for (std::size_t i{}; i < sample_count; ++i) {
            auto const index{(oldest_index + i) % sample_count};
            auto const sample{samples[index]};
            auto const x{static_cast<float>(sample.x() - first_x)};
            auto const y{static_cast<float>(sample.y())};
            auto const destination{static_cast<int32>(i)};
            changed = changed || x_[destination] != x || y_[destination] != y;
            x_[destination] = x;
            y_[destination] = y;
        }
    }

    if (changed) {
        update_slate_series();
    }
}

void UDebugGraphWidget::apply_hud_style(ioj::FGameHudStyle const& style) {
    graph_style_ = style.graph;
    series_colour_ = style.graph_series;
    if (graph_widget_.IsValid()) {
        (void)graph_widget_->set_style(graph_style_.GetValue());
        update_slate_series();
    }
}

TSharedRef<SWidget> UDebugGraphWidget::RebuildWidget() {
    SAssignNew(graph_widget_, SGraphPlot);
    if (graph_style_) {
        (void)graph_widget_->set_style(graph_style_.GetValue());
    }
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

    static FText const speed_series_name{FText::FromString(TEXT("SPEED"))};
    FGraphSeries series{
        .name = speed_series_name,
        .x = x_,
        .y = y_,
        .style = {.color = series_colour_, .thickness = 1.5f, .antialias = true},
    };
    TArray<FGraphSeries> series_snapshot;
    series_snapshot.Add(MoveTemp(series));
    graph_widget_->set_series(MoveTemp(series_snapshot));
}
