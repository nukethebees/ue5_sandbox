#include "STelemetryDashboardView.h"

#include "SandboxUI/widgets/SGraphPlot.h"
#include "SandboxUI/widgets/SHistogram.h"

#include <Framework/Application/SlateApplication.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Layout/SBorder.h>
#include <Widgets/Layout/SScrollBox.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/Text/STextBlock.h>

namespace ml::ioj {
namespace telemetry_dashboard_view {
auto text(FGameUiStyle const& style, FText value, EGameTextStyle const type = EGameTextStyle::Body)
    -> TSharedRef<SWidget> {
    return SNew(STextBlock).Text(MoveTemp(value)).TextStyle(&style.text(type)).AutoWrapText(true);
}

auto chart_title(FGameUiStyle const& style, FString value) -> TSharedRef<SWidget> {
    return text(style, FText::FromString(MoveTemp(value)), EGameTextStyle::Caption);
}

auto metric_panel(FGameUiStyle const& style,
                  FTelemetryRunAnalysis const& analysis,
                  ETelemetryDashboardMetric const metric,
                  FGraphPlotStyle const& graph_style,
                  FHistogramStyle const& histogram_style) -> TSharedRef<SWidget> {
    auto const* const series{analysis.find_metric(metric)};
    auto timeline{SNew(SGraphPlot).Style(graph_style)};
    auto histogram{SNew(SHistogram).Style(histogram_style)};
    if (series) {
        FGraphSeries graph_series{.name = FText::FromString(series->title),
                                  .x = series->real_elapsed_seconds,
                                  .y = series->values};
        graph_series.style.color = style.palette().honey;
        TArray<FGraphSeries> timeline_series;
        timeline_series.Add(MoveTemp(graph_series));
        timeline->set_series(MoveTemp(timeline_series));
        if (!series->values.IsEmpty()) {
            auto minimum{FMath::Min(series->values)};
            auto maximum{FMath::Max(series->values)};
            if (minimum == maximum) {
                minimum -= 0.5f;
                maximum += 0.5f;
            }
            histogram->set_bin_configuration(minimum, maximum, 20);
            histogram->set_samples(series->values);
        }
    }

    auto const title{series ? series->title : telemetry_metric_title(metric)};
    auto const units{series ? series->units : telemetry_metric_units(metric)};
    return SNew(SBorder)
        .BorderImage(&style.panel().background)
        .Padding(style.panel().padding)
            [SNew(SVerticalBox) +
             SVerticalBox::Slot().AutoHeight()[text(
                 style,
                 FText::FromString(FString::Printf(TEXT("%s // %s"), *title, *units)),
                 EGameTextStyle::Heading3)] +
             SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 2.0f)
                 [SNew(SHorizontalBox) +
                  SHorizontalBox::Slot().FillWidth(
                      0.62f)[chart_title(style, TEXT("TIMELINE // REAL ELAPSED TIME (s)"))] +
                  SHorizontalBox::Slot().FillWidth(0.38f).Padding(8.0f, 0.0f, 0.0f, 0.0f)
                      [chart_title(style, TEXT("DISTRIBUTION // ~1 HZ INTERVAL COUNTS"))]] +
             SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox) +
                                               SHorizontalBox::Slot().FillWidth(0.62f)[timeline] +
                                               SHorizontalBox::Slot().FillWidth(0.38f).Padding(
                                                   8.0f, 0.0f, 0.0f, 0.0f)[histogram]]];
}

auto metric_section(FGameUiStyle const& style,
                    FTelemetryRunAnalysis const& analysis,
                    FString title,
                    TConstArrayView<ETelemetryDashboardMetric> const metrics,
                    FString note,
                    FGraphPlotStyle const& graph_style,
                    FHistogramStyle const& histogram_style) -> TSharedRef<SWidget> {
    auto content{SNew(SVerticalBox)};
    content->AddSlot().AutoHeight().Padding(
        0.0f,
        12.0f,
        0.0f,
        4.0f)[text(style, FText::FromString(MoveTemp(title)), EGameTextStyle::Heading2)];
    if (!note.IsEmpty()) {
        content->AddSlot().AutoHeight().Padding(
            0.0f,
            0.0f,
            0.0f,
            6.0f)[text(style, FText::FromString(MoveTemp(note)), EGameTextStyle::BodySecondary)];
    }
    for (auto const metric : metrics) {
        content->AddSlot().AutoHeight().Padding(
            0.0f,
            0.0f,
            0.0f,
            8.0f)[metric_panel(style, analysis, metric, graph_style, histogram_style)];
    }
    return content;
}
} // namespace telemetry_dashboard_view

void STelemetryDashboardView::Construct(FArguments const& args) {
    style_ = args._Style;
    check(style_ != nullptr);
    on_refresh_ = args._OnRefresh;
    on_run_selected_ = args._OnRunSelected;
    on_level_filter_selected_ = args._OnLevelFilterSelected;
    ChildSlot[build()];
}

void STelemetryDashboardView::replace_state(FTelemetryDashboardViewState const& state) {
    state_ = state;
    ChildSlot[build()];
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

void STelemetryDashboardView::focus_primary_action() {
    if (primary_button_.IsValid()) {
        FSlateApplication::Get().SetKeyboardFocus(primary_button_);
    }
}

auto STelemetryDashboardView::build() -> TSharedRef<SWidget> {
    return SNew(SBorder)
        .BorderImage(&style_->chrome().body_background)
        .Padding(12.0f)[SNew(SHorizontalBox) +
                        SHorizontalBox::Slot().FillWidth(0.26f).Padding(
                            0.0f, 0.0f, 8.0f, 0.0f)[build_sidebar()] +
                        SHorizontalBox::Slot().FillWidth(0.74f)[build_detail()]];
}

auto STelemetryDashboardView::build_sidebar() -> TSharedRef<SWidget> {
    auto runs{SNew(SVerticalBox)};
    if (state_.runs.IsEmpty()) {
        auto message{!state_.directory_exists ? TEXT("Telemetry directory has not been created.")
                                              : TEXT("No runs match the current level filter.")};
        runs->AddSlot().AutoHeight().Padding(6.0f)[telemetry_dashboard_view::text(
            *style_, FText::FromString(message), EGameTextStyle::BodySecondary)];
    }
    for (auto const& run : state_.runs) {
        auto const selected{run.run_id == state_.selected_run_id};
        auto const label{
            FText::FromString(FString::Printf(TEXT("%s\n%s%s"),
                                              *run.level_label(),
                                              *run.launched_utc,
                                              selected ? TEXT("  // SELECTED") : TEXT("")))};
        runs->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[SNew(SButton).OnClicked(
            this, &STelemetryDashboardView::handle_run, run.run_id)[telemetry_dashboard_view::text(
            *style_, label, selected ? EGameTextStyle::Heading3 : EGameTextStyle::Body)]];
    }
    auto status{FString::Printf(
        TEXT("%d RUNS  //  %d UNREADABLE"), state_.runs.Num(), state_.unreadable_files)};
    return SNew(SVerticalBox) +
           SVerticalBox::Slot().AutoHeight()[telemetry_dashboard_view::text(
               *style_,
               NSLOCTEXT("TelemetryDashboard", "Title", "TELEMETRY"),
               EGameTextStyle::Heading2)] +
           SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[SNew(SButton).OnClicked(
               this, &STelemetryDashboardView::handle_filter)[telemetry_dashboard_view::text(
               *style_,
               FText::FromString(TEXT("LEVEL // ") + state_.selected_level_filter),
               EGameTextStyle::Caption)]] +
           SVerticalBox::Slot()
               .AutoHeight()[SAssignNew(primary_button_, SButton)
                                 .OnClicked(this, &STelemetryDashboardView::handle_refresh)
                                     [telemetry_dashboard_view::text(
                                         *style_,
                                         NSLOCTEXT("TelemetryDashboard", "Refresh", "REFRESH RUNS"),
                                         EGameTextStyle::Caption)]] +
           SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[telemetry_dashboard_view::text(
               *style_,
               FText::FromString(status),
               state_.unreadable_files > 0 ? EGameTextStyle::Warning : EGameTextStyle::Caption)] +
           SVerticalBox::Slot().FillHeight(1.0f)[SNew(SScrollBox) + SScrollBox::Slot()[runs]];
}

auto STelemetryDashboardView::build_detail() -> TSharedRef<SWidget> {
    detail_scroll_.Reset();
    timing_section_.Reset();
    workload_section_.Reset();
    activity_section_.Reset();
    queries_section_.Reset();
    if (!state_.error.IsEmpty()) {
        return telemetry_dashboard_view::text(
            *style_,
            FText::FromString(TEXT("INVALID SELECTED RUN\n") + state_.error),
            EGameTextStyle::Warning);
    }
    if (state_.selected_run_id.IsEmpty()) {
        return telemetry_dashboard_view::text(
            *style_,
            NSLOCTEXT("TelemetryDashboard", "NoRun", "NO TELEMETRY RUN SELECTED"),
            EGameTextStyle::BodySecondary);
    }
    auto graph_style{style_->hud().graph};
    graph_style.desired_size = {600.0f, 180.0f};
    graph_style.empty_text =
        NSLOCTEXT("TelemetryDashboard", "Insufficient", "Insufficient interval data");
    auto histogram_style{FHistogramStyle{}};
    histogram_style.desired_size = {400.0f, 180.0f};
    histogram_style.background_color = graph_style.background_color;
    histogram_style.plot_color = graph_style.plot_color;
    histogram_style.bar_color = style_->palette().honey;
    histogram_style.hovered_bar_color = style_->palette().honey_hovered;
    histogram_style.axis_color = style_->palette().border;
    histogram_style.label_color = style_->palette().text_muted;
    histogram_style.label_font = graph_style.label_font;
    histogram_style.empty_text = graph_style.empty_text;
    auto throughput{SNew(SGraphPlot).Style(graph_style)};
    FGraphSeries observed{.name = NSLOCTEXT("TelemetryDashboard", "Observed", "Observed"),
                          .x = state_.analysis.throughput_real_elapsed_seconds,
                          .y = state_.analysis.observed_time_scale};
    observed.style.color = style_->palette().honey;
    FGraphSeries requested{.name = NSLOCTEXT("TelemetryDashboard", "Requested", "Requested"),
                           .x = state_.analysis.throughput_real_elapsed_seconds,
                           .y = state_.analysis.requested_time_scale};
    requested.style.color = style_->palette().focus;
    requested.style.interpolation = EGraphSeriesInterpolation::StepAfter;
    TArray<FGraphSeries> throughput_series;
    throughput_series.Add(MoveTemp(observed));
    throughput_series.Add(MoveTemp(requested));
    throughput->set_series(MoveTemp(throughput_series));

    static constexpr ETelemetryDashboardMetric timing_metrics[]{
        ETelemetryDashboardMetric::RequestedTimeScaleRatio,
        ETelemetryDashboardMetric::ObservedTimeScale,
        ETelemetryDashboardMetric::TicksPerRealSecond,
        ETelemetryDashboardMetric::RealSampleInterval,
    };
    static constexpr ETelemetryDashboardMetric workload_metrics[]{
        ETelemetryDashboardMetric::ActiveEntities,
        ETelemetryDashboardMetric::PlayerShips,
        ETelemetryDashboardMetric::Turrets,
        ETelemetryDashboardMetric::CapitalShips,
        ETelemetryDashboardMetric::CapitalShipFighters,
        ETelemetryDashboardMetric::TubeSpinners,
        ETelemetryDashboardMetric::ActiveLasers,
        ETelemetryDashboardMetric::RegistrySlots,
        ETelemetryDashboardMetric::OccupiedSpatialCells,
    };
    static constexpr ETelemetryDashboardMetric activity_metrics[]{
        ETelemetryDashboardMetric::SpawnRate,
        ETelemetryDashboardMetric::DestructionRate,
        ETelemetryDashboardMetric::KillRate,
        ETelemetryDashboardMetric::LaserFireRate,
    };
    static constexpr ETelemetryDashboardMetric query_metrics[]{
        ETelemetryDashboardMetric::GridRebuildRate,
        ETelemetryDashboardMetric::RangeQueryRate,
        ETelemetryDashboardMetric::LineTraceRate,
        ETelemetryDashboardMetric::SweepTraceRate,
    };
    static_assert(UE_ARRAY_COUNT(timing_metrics) + UE_ARRAY_COUNT(workload_metrics) +
                      UE_ARRAY_COUNT(activity_metrics) + UE_ARRAY_COUNT(query_metrics) ==
                  static_cast<int32>(ETelemetryDashboardMetric::COUNT));

    timing_section_ = telemetry_dashboard_view::metric_section(
        *style_,
        state_.analysis,
        TEXT("TIMING"),
        timing_metrics,
        TEXT("Requested-scale ratio is observed time scale divided by requested time scale. "
             "100% matches the request; values above 100% ran faster than requested."),
        graph_style,
        histogram_style);
    workload_section_ = telemetry_dashboard_view::metric_section(*style_,
                                                                 state_.analysis,
                                                                 TEXT("WORKLOAD"),
                                                                 workload_metrics,
                                                                 {},
                                                                 graph_style,
                                                                 histogram_style);
    activity_section_ = telemetry_dashboard_view::metric_section(*style_,
                                                                 state_.analysis,
                                                                 TEXT("ACTIVITY RATES"),
                                                                 activity_metrics,
                                                                 {},
                                                                 graph_style,
                                                                 histogram_style);
    queries_section_ = telemetry_dashboard_view::metric_section(*style_,
                                                                state_.analysis,
                                                                TEXT("QUERY RATES"),
                                                                query_metrics,
                                                                {},
                                                                graph_style,
                                                                histogram_style);

    auto content{SNew(SVerticalBox)};
    content->AddSlot().AutoHeight()[telemetry_dashboard_view::text(
        *style_, state_.header, EGameTextStyle::Heading3)];
    content->AddSlot().AutoHeight().Padding(
        0.0f, 8.0f)[SNew(SBorder)
                        .BorderImage(&style_->panel().background)
                        .Padding(style_->panel().padding)[telemetry_dashboard_view::text(
                            *style_, state_.summary, EGameTextStyle::HudPrimary)]];
    content->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)[telemetry_dashboard_view::text(
        *style_,
        NSLOCTEXT("TelemetryDashboard",
                  "WarmupIntervalOmitted",
                  "INTERVAL ANALYSIS // FIRST ~1 HZ WARM-UP INTERVAL OMITTED"),
        EGameTextStyle::BodySecondary)];
    content->AddSlot().AutoHeight().Padding(0.0f, 6.0f)[telemetry_dashboard_view::chart_title(
        *style_, TEXT("THROUGHPUT // TIME SCALE (x) BY REAL ELAPSED TIME (s)"))];
    content->AddSlot().AutoHeight()[throughput];
    content->AddSlot().AutoHeight()[timing_section_.ToSharedRef()];
    content->AddSlot().AutoHeight()[workload_section_.ToSharedRef()];
    content->AddSlot().AutoHeight()[activity_section_.ToSharedRef()];
    content->AddSlot().AutoHeight()[queries_section_.ToSharedRef()];

    auto navigation{SNew(SHorizontalBox)};
    auto add_jump = [this, &navigation](FText label, ETelemetryDashboardSection const section) {
        navigation->AddSlot().FillWidth(1.0f).Padding(
            0.0f, 0.0f, 4.0f, 0.0f)[SNew(SButton).OnClicked(
            this, &STelemetryDashboardView::handle_section, section)[telemetry_dashboard_view::text(
            *style_, MoveTemp(label), EGameTextStyle::Caption)]];
    };
    add_jump(NSLOCTEXT("TelemetryDashboard", "OverviewSection", "OVERVIEW"),
             ETelemetryDashboardSection::Overview);
    add_jump(NSLOCTEXT("TelemetryDashboard", "TimingSection", "TIMING"),
             ETelemetryDashboardSection::Timing);
    add_jump(NSLOCTEXT("TelemetryDashboard", "WorkloadSection", "WORKLOAD"),
             ETelemetryDashboardSection::Workload);
    add_jump(NSLOCTEXT("TelemetryDashboard", "ActivitySection", "ACTIVITY"),
             ETelemetryDashboardSection::Activity);
    add_jump(NSLOCTEXT("TelemetryDashboard", "QueriesSection", "QUERIES"),
             ETelemetryDashboardSection::Queries);

    return SNew(SVerticalBox) +
           SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[navigation] +
           SVerticalBox::Slot().FillHeight(1.0f)[SAssignNew(detail_scroll_, SScrollBox)
                                                     .ScrollBarStyle(&style_->chrome().scroll_bar) +
                                                 SScrollBox::Slot()[content]];
}

auto STelemetryDashboardView::handle_refresh() -> FReply {
    on_refresh_.ExecuteIfBound();
    return FReply::Handled();
}
auto STelemetryDashboardView::handle_run(FString run_id) -> FReply {
    on_run_selected_.ExecuteIfBound(MoveTemp(run_id));
    return FReply::Handled();
}
auto STelemetryDashboardView::handle_filter() -> FReply {
    auto index{state_.level_filters.IndexOfByKey(state_.selected_level_filter)};
    if (!state_.level_filters.IsEmpty()) {
        index = (index + 1) % state_.level_filters.Num();
        on_level_filter_selected_.ExecuteIfBound(state_.level_filters[index]);
    }
    return FReply::Handled();
}
auto STelemetryDashboardView::handle_section(ETelemetryDashboardSection const section) -> FReply {
    if (!detail_scroll_.IsValid()) {
        return FReply::Handled();
    }
    if (section == ETelemetryDashboardSection::Overview) {
        detail_scroll_->ScrollToStart();
        return FReply::Handled();
    }
    TSharedPtr<SWidget> target;
    switch (section) {
        case ETelemetryDashboardSection::Overview:
            break;
        case ETelemetryDashboardSection::Timing:
            target = timing_section_;
            break;
        case ETelemetryDashboardSection::Workload:
            target = workload_section_;
            break;
        case ETelemetryDashboardSection::Activity:
            target = activity_section_;
            break;
        case ETelemetryDashboardSection::Queries:
            target = queries_section_;
            break;
    }
    if (target.IsValid()) {
        detail_scroll_->ScrollDescendantIntoView(
            target, true, EDescendantScrollDestination::TopOrLeft);
    }
    return FReply::Handled();
}
} // namespace ml::ioj
