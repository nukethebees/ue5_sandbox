#include "LevelTelemetryPresentation.h"

#include "SandboxUI/widgets/SGraphPlot.h"

namespace {
template <typename Data>
auto make_graph_series(FText name,
                       Data const& data,
                       ioj::Frequency const tick_rate,
                       FLinearColor const color) -> FGraphSeries {
    FGraphSeries series;
    series.name = MoveTemp(name);
    series.style = {.color = color,
                    .thickness = 1.5f,
                    .antialias = true,
                    .interpolation = EGraphSeriesInterpolation::StepAfter};

    auto const sample_count{static_cast<int32>(data.num())};
    series.x.Reserve(sample_count);
    series.y.Reserve(sample_count);
    auto const tick_rate_hz{tick_rate.get_hz()};
    for (int32 index{}; index < sample_count; ++index) {
        series.x.Add(static_cast<float>(static_cast<double>(data.time_at(index)) / tick_rate_hz));
        series.y.Add(static_cast<float>(data.value_at(index)));
    }
    return series;
}
}

namespace ioj::level_telemetry_presentation {
auto format_elapsed_time(Duration const elapsed) -> FText {
    auto const total_seconds{FMath::FloorToInt64(elapsed.get_seconds())};
    auto const seconds{total_seconds % 60};
    auto const total_minutes{total_seconds / 60};
    auto const minutes{total_minutes % 60};
    auto const hours{total_minutes / 60};
    if (hours > 0) {
        return FText::FromString(
            FString::Printf(TEXT("%lld:%02lld:%02lld"), hours, minutes, seconds));
    }
    return FText::FromString(FString::Printf(TEXT("%02lld:%02lld"), minutes, seconds));
}

void apply_activity_graph_style(SGraphPlot& graph,
                                FGameUiStyle const& style,
                                FText empty_text,
                                FVector2f const desired_size) {
    auto graph_style{FGraphPlotStyle{}};
    graph_style.desired_size = desired_size;
    graph_style.label_font = style.text(EGameTextStyle::Caption).Font;
    graph_style.label_color =
        style.text(EGameTextStyle::Caption).ColorAndOpacity.GetSpecifiedColor();
    graph_style.axis_color =
        style.text(EGameTextStyle::BodySecondary).ColorAndOpacity.GetSpecifiedColor();
    graph_style.grid_color = graph_style.axis_color.CopyWithNewOpacity(0.25f);
    auto const panel_color{style.panel().background.TintColor.GetSpecifiedColor()};
    graph_style.background_color = panel_color;
    graph_style.plot_color = panel_color.CopyWithNewOpacity(panel_color.A * 0.35f);
    graph_style.empty_text = MoveTemp(empty_text);
    (void)graph.set_style(MoveTemp(graph_style));
}

void update_activity_graph(SGraphPlot& graph,
                           sim::LevelTelemetrySnapshot const& snapshot,
                           FLinearColor const active_entity_color,
                           FLinearColor const kills_color) {
    if (snapshot.active_entity_count_data.is_empty() &&
        snapshot.cumulative_kill_count_data.is_empty()) {
        graph.clear_series();
        return;
    }

    TArray<FGraphSeries> series;
    series.Reserve(2);
    series.Add(make_graph_series(NSLOCTEXT("LevelTelemetry", "ActiveEntities", "Active entities"),
                                 snapshot.active_entity_count_data,
                                 snapshot.tick_rate_hz,
                                 active_entity_color));
    series.Add(make_graph_series(NSLOCTEXT("LevelTelemetry", "Kills", "Kills"),
                                 snapshot.cumulative_kill_count_data,
                                 snapshot.tick_rate_hz,
                                 kills_color));
    graph.set_series(MoveTemp(series));

    auto const x_max{FMath::Max(snapshot.elapsed_seconds.get_seconds(), 1.0)};
    (void)graph.set_axis_settings(
        {.range_mode = EGraphRangeMode::Fixed, .fixed_range = {0.0, x_max}},
        {.range_mode = EGraphRangeMode::AutoIncludeZero});
}
}
