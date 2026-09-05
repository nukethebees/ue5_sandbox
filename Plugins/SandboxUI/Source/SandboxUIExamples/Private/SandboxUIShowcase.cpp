#include "SandboxUIExamples/SandboxUIShowcase.h"

#include "SandboxUI/widgets/SGraphPlot.h"
#include "SandboxUI/widgets/SHeatmap2D.h"
#include "SandboxUI/widgets/SHistogram.h"
#include "SandboxUI/widgets/SRadar2D.h"
#include "SandboxUI/widgets/SScatterPlot.h"
#include "SandboxUI/widgets/SStackedBarChart.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/NativeWidgetHost.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#include "generated/USandboxUIShowcase.slate.generated.h"

DEFINE_LOG_CATEGORY_STATIC(LogSandboxUIExamples, Log, All);

namespace {
using ShowcaseBuilder = SlateGenerated::USandboxUIShowcaseBuilder;

FName const radar_multi_host_name{TEXT("radar_multi_host")};
FName const radar_dense_host_name{TEXT("radar_dense_host")};
FName const graph_series_host_name{TEXT("graph_series_host")};
FName const graph_dense_host_name{TEXT("graph_dense_host")};
FName const histogram_host_name{TEXT("histogram_host")};
FName const scatter_host_name{TEXT("scatter_host")};
FName const stacked_bar_host_name{TEXT("stacked_bar_host")};
FName const heatmap_smooth_host_name{TEXT("heatmap_smooth_host")};
FName const heatmap_sparse_host_name{TEXT("heatmap_sparse_host")};

auto construct_text(UWidgetTree& widget_tree,
                    FName const name,
                    FText const& text,
                    int32 const font_size,
                    FLinearColor const color) -> UTextBlock* {
    auto* const text_block{
        widget_tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), name)};
    check(text_block);
    text_block->SetText(text);
    text_block->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", font_size));
    text_block->SetColorAndOpacity(color);
    text_block->SetAutoWrapText(true);
    return text_block;
}

void add_showcase_card(UWidgetTree& widget_tree,
                       UVerticalBox& gallery,
                       FName const card_name,
                       FName const host_name,
                       FText const& title,
                       FText const& description,
                       float const content_height) {
    auto* const card{widget_tree.ConstructWidget<UBorder>(UBorder::StaticClass(), card_name)};
    check(card);
    card->SetPadding(FMargin{16.0f});
    card->SetBrushColor({0.025f, 0.032f, 0.045f, 1.0f});

    auto* const card_contents{widget_tree.ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), FName{card_name.ToString() + TEXT("_contents")})};
    check(card_contents);
    card->SetContent(card_contents);

    auto* const title_text{construct_text(widget_tree,
                                          FName{card_name.ToString() + TEXT("_title")},
                                          title,
                                          16,
                                          {0.92f, 0.94f, 1.0f, 1.0f})};
    auto* const title_slot{card_contents->AddChildToVerticalBox(title_text)};
    check(title_slot);
    title_slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 4.0f});

    auto* const description_text{construct_text(widget_tree,
                                                FName{card_name.ToString() + TEXT("_description")},
                                                description,
                                                10,
                                                {0.65f, 0.7f, 0.78f, 1.0f})};
    auto* const description_slot{card_contents->AddChildToVerticalBox(description_text)};
    check(description_slot);
    description_slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 10.0f});

    auto* const content_size{widget_tree.ConstructWidget<USizeBox>(
        USizeBox::StaticClass(), FName{card_name.ToString() + TEXT("_size")})};
    check(content_size);
    content_size->SetHeightOverride(content_height);
    auto* const content_slot{card_contents->AddChildToVerticalBox(content_size)};
    check(content_slot);
    content_slot->SetHorizontalAlignment(HAlign_Fill);

    auto* const host{widget_tree.ConstructWidget<UNativeWidgetHost>(
        UNativeWidgetHost::StaticClass(), host_name)};
    check(host);
    content_size->SetContent(host);

    auto* const gallery_slot{gallery.AddChildToVerticalBox(card)};
    check(gallery_slot);
    gallery_slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 14.0f});
    gallery_slot->SetHorizontalAlignment(HAlign_Fill);
}

auto find_showcase_host(UWidgetTree& widget_tree, FName const host_name) -> UNativeWidgetHost* {
    auto* const host{Cast<UNativeWidgetHost>(widget_tree.FindWidget(host_name))};
    if (host == nullptr) {
        UE_LOG(LogSandboxUIExamples,
               Error,
               TEXT("SandboxUI showcase could not find NativeWidgetHost '%s'."),
               *host_name.ToString());
    }
    return host;
}

void set_showcase_content(UWidgetTree& widget_tree,
                          FName const host_name,
                          TSharedRef<SWidget> content) {
    auto* const host{find_showcase_host(widget_tree, host_name)};
    if (host == nullptr) {
        return;
    }
    host->SetContent(MoveTemp(content));
}

auto make_radar_style(FLinearColor const color, FVector2f const size) -> FRadar2DContactStyle {
    FRadar2DContactStyle style;
    style.tint = color;
    style.rendered_size = size;
    return style;
}

auto make_multi_style_radar(ShowcaseBuilder& builder) -> TSharedRef<SWidget> {
    FRadar2DPresentation presentation;
    presentation.desired_size = {360.0f, 320.0f};

    auto radar{builder.BuildRadar(100.0f, presentation)};
    TArray<FRadar2DStyleBucket> buckets;

    FRadar2DStyleBucket cyan_bucket;
    cyan_bucket.style = make_radar_style({0.1f, 0.85f, 1.0f, 1.0f}, {7.0f, 7.0f});
    for (FVector2f const position : {FVector2f{-72.0f, 42.0f},
                                     FVector2f{-45.0f, -18.0f},
                                     FVector2f{-12.0f, 68.0f},
                                     FVector2f{18.0f, 24.0f},
                                     FVector2f{63.0f, -52.0f}}) {
        cyan_bucket.positions.add(position);
    }
    buckets.Add(MoveTemp(cyan_bucket));

    FRadar2DStyleBucket amber_bucket;
    amber_bucket.style = make_radar_style({1.0f, 0.62f, 0.08f, 1.0f}, {10.0f, 10.0f});
    for (FVector2f const position :
         {FVector2f{-34.0f, 8.0f}, FVector2f{6.0f, -12.0f}, FVector2f{47.0f, 44.0f}}) {
        amber_bucket.positions.add(position);
    }
    buckets.Add(MoveTemp(amber_bucket));

    FRadar2DStyleBucket white_bucket;
    white_bucket.style = make_radar_style(FLinearColor::White, {5.0f, 5.0f});
    white_bucket.positions.add(0.0f, 0.0f);
    buckets.Add(MoveTemp(white_bucket));

    (void)radar->set_buckets(MoveTemp(buckets));
    return radar;
}

auto make_dense_radar(ShowcaseBuilder& builder) -> TSharedRef<SWidget> {
    auto radar{builder.BuildRadarWithRange(100.0f)};
    TArray<FRadar2DStyleBucket> buckets;
    buckets.SetNum(3);
    buckets[0].style = make_radar_style({0.18f, 0.8f, 1.0f, 0.9f}, {4.0f, 4.0f});
    buckets[1].style = make_radar_style({0.8f, 0.3f, 1.0f, 0.9f}, {5.0f, 5.0f});
    buckets[2].style = make_radar_style({1.0f, 0.25f, 0.18f, 0.95f}, {7.0f, 7.0f});

    int32 constexpr contact_count{360};
    for (int32 contact_index{0}; contact_index < contact_count; ++contact_index) {
        auto const angle{static_cast<float>(contact_index) * 2.399963f};
        auto const radius{8.0f + static_cast<float>(contact_index % 120)};
        buckets[contact_index % buckets.Num()].positions.add(FMath::Cos(angle) * radius,
                                                             FMath::Sin(angle) * radius);
    }

    (void)radar->set_buckets(MoveTemp(buckets));
    return radar;
}

auto make_time_series_graph(ShowcaseBuilder& builder) -> TSharedRef<SWidget> {
    auto graph{builder.BuildGraph()};
    TArray<FGraphSeries> series;
    FGraphSeries throughput;
    throughput.name = NSLOCTEXT("SandboxUIShowcase", "Throughput", "Throughput");
    throughput.style = {.color = {0.1f, 0.85f, 1.0f, 1.0f}, .thickness = 1.5f, .antialias = true};
    FGraphSeries latency;
    latency.name = NSLOCTEXT("SandboxUIShowcase", "Latency", "Latency");
    latency.style = {.color = {1.0f, 0.45f, 0.12f, 1.0f},
                     .thickness = 1.5f,
                     .antialias = true,
                     .interpolation = EGraphSeriesInterpolation::StepAfter};

    int32 constexpr sample_count{240};
    throughput.x.Reserve(sample_count);
    throughput.y.Reserve(sample_count);
    latency.x.Reserve(sample_count);
    latency.y.Reserve(sample_count);
    for (int32 sample_index{0}; sample_index < sample_count; ++sample_index) {
        auto const time{static_cast<float>(sample_index) * 0.1f};
        throughput.x.Add(time);
        throughput.y.Add(58.0f + FMath::Sin(time * 0.9f) * 17.0f + FMath::Sin(time * 2.7f) * 4.0f);
        latency.x.Add(time);
        latency.y.Add(32.0f + FMath::Cos(time * 0.65f) * 9.0f + FMath::Sin(time * 1.8f) * 3.0f);
    }
    series.Add(MoveTemp(throughput));
    series.Add(MoveTemp(latency));
    graph->set_series(MoveTemp(series));
    return graph;
}

auto make_dense_graph(ShowcaseBuilder& builder) -> TSharedRef<SWidget> {
    FGraphAxisSettings const x_axis{.range_mode = EGraphRangeMode::Fixed,
                                    .fixed_range = {0.0, 60.0}};
    FGraphAxisSettings const y_axis{.range_mode = EGraphRangeMode::AutoIncludeZero};
    auto graph{builder.BuildGraphWithAxes(x_axis, y_axis)};

    FGraphSeries activity;
    activity.name = NSLOCTEXT("SandboxUIShowcase", "Activity", "Activity");
    activity.style = {.color = {0.45f, 1.0f, 0.35f, 1.0f}, .thickness = 1.0f, .antialias = false};
    int32 constexpr sample_count{2048};
    activity.x.Reserve(sample_count);
    activity.y.Reserve(sample_count);
    for (int32 sample_index{0}; sample_index < sample_count; ++sample_index) {
        auto const time{60.0f * static_cast<float>(sample_index) /
                        static_cast<float>(sample_count - 1)};
        auto value{18.0f + FMath::Sin(time * 3.8f) * 5.0f + FMath::Sin(time * 13.0f) * 1.5f};
        if (sample_index == 1327) {
            value = 92.0f;
        }
        activity.x.Add(time);
        activity.y.Add(value);
    }
    TArray<FGraphSeries> series;
    series.Add(MoveTemp(activity));
    graph->set_series(MoveTemp(series));
    return graph;
}

auto make_histogram(ShowcaseBuilder& builder) -> TSharedRef<SWidget> {
    auto histogram{builder.BuildHistogram()};
    TArray<float> samples;
    int32 constexpr sample_count{320};
    samples.Reserve(sample_count);
    for (int32 sample_index{0}; sample_index < sample_count; ++sample_index) {
        auto const phase{static_cast<float>(sample_index) * 1.618034f};
        auto const wobble{FMath::Sin(phase) * 0.72f + FMath::Sin(phase * 2.37f) * 0.22f};
        auto const centre{sample_index < 190 ? -1.15f : 1.45f};
        auto const scale{sample_index < 190 ? 1.0f : 0.7f};
        samples.Add(centre + wobble * scale);
    }
    histogram->set_samples(MoveTemp(samples));
    return histogram;
}

auto make_scatter_plot(ShowcaseBuilder& builder) -> TSharedRef<SWidget> {
    FScatterPlotDomain const domain{-10.0f, 10.0f, -8.0f, 8.0f};
    auto scatter{builder.BuildScatter(domain)};
    TArray<FScatterPlotPoint> points;
    points.Reserve(160);

    struct FCluster {
        FVector2f centre;
        FVector2f extent;
        FLinearColor color;
    };
    TArray<FCluster> const clusters{{{-4.2f, 2.0f}, {2.2f, 1.4f}, {0.1f, 0.8f, 1.0f, 0.9f}},
                                    {{3.6f, 2.7f}, {1.8f, 2.0f}, {1.0f, 0.45f, 0.1f, 0.9f}},
                                    {{1.2f, -3.4f}, {2.6f, 1.2f}, {0.55f, 1.0f, 0.3f, 0.9f}}};
    int32 constexpr points_per_cluster{50};
    for (int32 cluster_index{0}; cluster_index < clusters.Num(); ++cluster_index) {
        auto const& cluster{clusters[cluster_index]};
        for (int32 point_index{0}; point_index < points_per_cluster; ++point_index) {
            auto const angle{static_cast<float>(point_index) * 2.399963f};
            auto const radius{FMath::Sqrt(static_cast<float>(point_index + 1) /
                                          static_cast<float>(points_per_cluster))};
            points.Add(
                {.position = cluster.centre + FVector2f{FMath::Cos(angle) * cluster.extent.X,
                                                        FMath::Sin(angle) * cluster.extent.Y} *
                                                  radius,
                 .color = cluster.color});
        }
    }
    points.Add({.position = {-8.5f, -6.5f}, .color = FLinearColor::White});
    points.Add({.position = {8.8f, 6.2f}, .color = FLinearColor::White});
    scatter->set_points(MoveTemp(points));
    return scatter;
}

auto make_stacked_bar_chart(ShowcaseBuilder& builder) -> TSharedRef<SWidget> {
    auto chart{builder.BuildStackedBarChart()};
    auto const blue{FLinearColor{0.1f, 0.55f, 1.0f, 1.0f}};
    auto const amber{FLinearColor{1.0f, 0.55f, 0.08f, 1.0f}};
    auto const violet{FLinearColor{0.65f, 0.25f, 1.0f, 1.0f}};
    TArray<FStackedBar> bars{
        {.label = FText::FromString(TEXT("T+0")), .segments = {{22.0f, blue}, {12.0f, amber}}},
        {.label = FText::FromString(TEXT("T+1")),
         .segments = {{28.0f, blue}, {18.0f, amber}, {7.0f, violet}}},
        {.label = FText::FromString(TEXT("T+2")),
         .segments = {{35.0f, blue}, {14.0f, amber}, {15.0f, violet}}},
        {.label = FText::FromString(TEXT("T+3")),
         .segments = {{30.0f, blue}, {27.0f, amber}, {12.0f, violet}}},
        {.label = FText::FromString(TEXT("T+4")),
         .segments = {{42.0f, blue}, {21.0f, amber}, {18.0f, violet}}},
        {.label = FText::FromString(TEXT("T+5")),
         .segments = {{38.0f, blue}, {16.0f, amber}, {10.0f, violet}}},
    };
    chart->set_bars(MoveTemp(bars));
    return chart;
}

auto heat_peak(float const x,
               float const y,
               float const centre_x,
               float const centre_y,
               float const sharpness) -> float {
    auto const dx{x - centre_x};
    auto const dy{y - centre_y};
    return FMath::Exp(-(dx * dx + dy * dy) * sharpness);
}

auto make_smooth_heatmap(ShowcaseBuilder& builder) -> TSharedRef<SWidget> {
    FHeatmapDomain const domain{0.0f, 320.0f, 0.0f, 240.0f};
    auto heatmap{builder.BuildHeatmap(FHeatmapValueRange{0.0f, 1.0f}, domain)};
    FHeatmapGrid grid{.columns = 32, .rows = 24};
    grid.values.Reserve(grid.columns * grid.rows);
    for (int32 row{0}; row < grid.rows; ++row) {
        auto const y{static_cast<float>(row) / static_cast<float>(grid.rows - 1)};
        for (int32 column{0}; column < grid.columns; ++column) {
            auto const x{static_cast<float>(column) / static_cast<float>(grid.columns - 1)};
            auto const value{0.08f + heat_peak(x, y, 0.28f, 0.35f, 28.0f) * 0.78f +
                             heat_peak(x, y, 0.72f, 0.68f, 45.0f) * 0.95f + x * 0.08f};
            grid.values.Add(FMath::Clamp(value, 0.0f, 1.0f));
        }
    }
    (void)heatmap->set_grid(MoveTemp(grid));
    return heatmap;
}

auto make_sparse_heatmap(ShowcaseBuilder& builder) -> TSharedRef<SWidget> {
    FHeatmapDomain const domain{-120.0f, 120.0f, -80.0f, 80.0f};
    auto heatmap{builder.BuildHeatmap(FHeatmapValueRange{0.18f, 1.0f}, domain)};
    FHeatmapGrid grid{.columns = 30, .rows = 20};
    grid.values.Reserve(grid.columns * grid.rows);
    for (int32 row{0}; row < grid.rows; ++row) {
        auto const y{static_cast<float>(row) / static_cast<float>(grid.rows - 1)};
        for (int32 column{0}; column < grid.columns; ++column) {
            auto const x{static_cast<float>(column) / static_cast<float>(grid.columns - 1)};
            auto const value{FMath::Max3(heat_peak(x, y, 0.18f, 0.72f, 180.0f),
                                         heat_peak(x, y, 0.55f, 0.35f, 260.0f),
                                         heat_peak(x, y, 0.82f, 0.62f, 220.0f))};
            grid.values.Add(value);
        }
    }
    (void)heatmap->set_grid(MoveTemp(grid));
    return heatmap;
}
} // namespace

TSharedRef<SWidget> USandboxUIShowcase::RebuildWidget() {
    if (WidgetTree == nullptr) {
        UE_LOG(LogSandboxUIExamples, Error, TEXT("SandboxUI showcase has no WidgetTree."));
        return Super::RebuildWidget();
    }
    if (WidgetTree->FindWidget(TEXT("gallery_contents")) == nullptr) {
        WidgetTree->RootWidget = nullptr;
        build_widget_tree();
    }
    populate_examples();
    return Super::RebuildWidget();
}

void USandboxUIShowcase::build_widget_tree() {
    check(WidgetTree);
    check(WidgetTree->RootWidget == nullptr);

    auto* const root_size{
        WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("root_size"))};
    check(root_size);
    root_size->SetWidthOverride(860.0f);
    root_size->SetHeightOverride(900.0f);
    WidgetTree->RootWidget = root_size;

    auto* const scroll_box{
        WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("gallery_scroll"))};
    check(scroll_box);
    root_size->SetContent(scroll_box);

    auto* const page_border{
        WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("page_border"))};
    check(page_border);
    page_border->SetPadding(FMargin{22.0f});
    page_border->SetBrushColor({0.008f, 0.011f, 0.018f, 1.0f});
    auto* const page_slot{Cast<UScrollBoxSlot>(scroll_box->AddChild(page_border))};
    check(page_slot);
    page_slot->SetHorizontalAlignment(HAlign_Fill);

    auto* const gallery{WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
                                                                  TEXT("gallery_contents"))};
    check(gallery);
    page_border->SetContent(gallery);

    auto* const heading{
        construct_text(*WidgetTree,
                       TEXT("showcase_heading"),
                       NSLOCTEXT("SandboxUIShowcase", "Heading", "SandboxUI Widget Showcase"),
                       24,
                       {0.95f, 0.97f, 1.0f, 1.0f})};
    heading->SetJustification(ETextJustify::Center);
    auto* const heading_slot{gallery->AddChildToVerticalBox(heading)};
    check(heading_slot);
    heading_slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 6.0f});

    auto* const intro{construct_text(
        *WidgetTree,
        TEXT("showcase_intro"),
        NSLOCTEXT("SandboxUIShowcase",
                  "Intro",
                  "Live, deterministic examples of the reusable Slate widgets in SandboxUI."),
        11,
        {0.65f, 0.7f, 0.78f, 1.0f})};
    intro->SetJustification(ETextJustify::Center);
    auto* const intro_slot{gallery->AddChildToVerticalBox(intro)};
    check(intro_slot);
    intro_slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 18.0f});

    add_showcase_card(
        *WidgetTree,
        *gallery,
        TEXT("radar_multi_card"),
        radar_multi_host_name,
        FText::FromString(TEXT("SRadar2D - Multi-style contacts")),
        FText::FromString(TEXT("Three owned style buckets with distinct tints and sizes.")),
        320.0f);
    add_showcase_card(
        *WidgetTree,
        *gallery,
        TEXT("radar_dense_card"),
        radar_dense_host_name,
        FText::FromString(TEXT("SRadar2D - Dense field and clipping")),
        FText::FromString(TEXT("A dense spiral extending beyond the configured radar range.")),
        320.0f);
    add_showcase_card(*WidgetTree,
                      *gallery,
                      TEXT("graph_series_card"),
                      graph_series_host_name,
                      FText::FromString(TEXT("SGraphPlot - Multiple time series")),
                      FText::FromString(TEXT(
                          "Linear and step-after series sharing automatically resolved axes.")),
                      260.0f);
    add_showcase_card(
        *WidgetTree,
        *gallery,
        TEXT("graph_dense_card"),
        graph_dense_host_name,
        FText::FromString(TEXT("SGraphPlot - Dense series and spike")),
        FText::FromString(TEXT(
            "2,048 samples exercise resolution-aware decimation while preserving a narrow spike.")),
        260.0f);
    add_showcase_card(
        *WidgetTree,
        *gallery,
        TEXT("histogram_card"),
        histogram_host_name,
        FText::FromString(TEXT("SHistogram - Bimodal distribution")),
        FText::FromString(TEXT("A deterministic two-cluster sample distribution across 24 bins.")),
        240.0f);
    add_showcase_card(*WidgetTree,
                      *gallery,
                      TEXT("scatter_card"),
                      scatter_host_name,
                      FText::FromString(TEXT("SScatterPlot - Clusters and outliers")),
                      FText::FromString(TEXT(
                          "Three coloured clusters plus isolated points across the full domain.")),
                      260.0f);
    add_showcase_card(
        *WidgetTree,
        *gallery,
        TEXT("stacked_bar_card"),
        stacked_bar_host_name,
        FText::FromString(TEXT("SStackedBarChart - Segment composition")),
        FText::FromString(TEXT("Labelled bars with ordered segments and differing totals.")),
        240.0f);
    add_showcase_card(
        *WidgetTree,
        *gallery,
        TEXT("heatmap_smooth_card"),
        heatmap_smooth_host_name,
        FText::FromString(TEXT("SHeatmap2D - Smooth spatial field")),
        FText::FromString(TEXT("A 32 x 24 dense field with gradients and overlapping hotspots.")),
        300.0f);
    add_showcase_card(
        *WidgetTree,
        *gallery,
        TEXT("heatmap_sparse_card"),
        heatmap_sparse_host_name,
        FText::FromString(TEXT("SHeatmap2D - Sparse hotspots")),
        FText::FromString(TEXT("A transparent floor with isolated high-value regions.")),
        300.0f);
}

void USandboxUIShowcase::populate_examples() {
    check(WidgetTree);
    auto builder{ShowcaseBuilder{*this}};
    set_showcase_content(*WidgetTree, radar_multi_host_name, make_multi_style_radar(builder));
    set_showcase_content(*WidgetTree, radar_dense_host_name, make_dense_radar(builder));
    set_showcase_content(*WidgetTree, graph_series_host_name, make_time_series_graph(builder));
    set_showcase_content(*WidgetTree, graph_dense_host_name, make_dense_graph(builder));
    set_showcase_content(*WidgetTree, histogram_host_name, make_histogram(builder));
    set_showcase_content(*WidgetTree, scatter_host_name, make_scatter_plot(builder));
    set_showcase_content(*WidgetTree, stacked_bar_host_name, make_stacked_bar_chart(builder));
    set_showcase_content(*WidgetTree, heatmap_smooth_host_name, make_smooth_heatmap(builder));
    set_showcase_content(*WidgetTree, heatmap_sparse_host_name, make_sparse_heatmap(builder));
}
