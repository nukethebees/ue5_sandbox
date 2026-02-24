#include "Sandbox/ui/widgets/DebugGraphWidget.h"

#include "Styling/CoreStyle.h"

#include <limits>

void UDebugGraphWidget::set_samples(std::span<FVector2d> in_samples, int32 oldest_index) {
    samples.Reset();
    samples.AddZeroed(in_samples.size());

    for (int32 i{0}; i < samples.Num(); ++i) {
        auto const index{(oldest_index + i) % static_cast<int32>(in_samples.size())};
        samples[i] = in_samples[index];
    }

    Invalidate(EInvalidateWidget::Paint);
}

int32 UDebugGraphWidget::NativePaint(FPaintArgs const& args,
                                     FGeometry const& geometry,
                                     FSlateRect const& culling_rect,
                                     FSlateWindowElementList& out_draw_elements,
                                     int32 layer_id,
                                     FWidgetStyle const& widget_style,
                                     bool parent_enabled) const {
    auto const size{geometry.GetLocalSize()};

    if (size.X <= 1.0f || size.Y <= 1.0f) {
        return layer_id;
    }

    constexpr float padding{6.0f};
    FVector2d const origin{padding, padding};
    FVector2d const graph_size{size.X - padding * 2.0f, size.Y - padding * 2.0f};

    // ------------------------------------------------------------
    // Background
    // ------------------------------------------------------------
    FSlateDrawElement::MakeBox(out_draw_elements,
                               layer_id,
                               geometry.ToPaintGeometry(),
                               FCoreStyle::Get().GetBrush("WhiteBrush"),
                               ESlateDrawEffect::None,
                               FLinearColor(0.0f, 0.0f, 0.0f, 0.4f));

    ++layer_id;

    // ------------------------------------------------------------
    // Axes
    // ------------------------------------------------------------
    TArray<FVector2D> axis_lines;
    axis_lines.Reserve(4);

    // X axis
    axis_lines.Add(origin);
    axis_lines.Add(origin + FVector2D{graph_size.X, 0.0f});

    // Y axis
    axis_lines.Add(origin);
    axis_lines.Add(origin + FVector2D{0.0f, graph_size.Y});

    FSlateDrawElement::MakeLines(out_draw_elements,
                                 layer_id,
                                 geometry.ToPaintGeometry(),
                                 axis_lines,
                                 ESlateDrawEffect::None,
                                 FLinearColor::White,
                                 true,
                                 1.0f);

    ++layer_id;

    // ------------------------------------------------------------
    // Graph line
    // ------------------------------------------------------------
    if (samples.Num() >= 2) {
        TArray<FVector2D> points;
        points.Reserve(samples.Num());

        for (int32 i = 0; i < samples.Num(); ++i) {
            auto& sample{samples[i]};
            float const x_alpha = static_cast<float>(i) / (samples.Num() - 1);
            auto const y_alpha{sample.Y / 10e3};

            points.Emplace(origin.X + x_alpha * graph_size.X,
                           origin.Y + (1.0f - y_alpha) * graph_size.Y);
        }

        FSlateDrawElement::MakeLines(out_draw_elements,
                                     layer_id,
                                     geometry.ToPaintGeometry(),
                                     points,
                                     ESlateDrawEffect::None,
                                     FLinearColor::Green,
                                     true,
                                     1.5f);

        ++layer_id;
    }

    return layer_id;
}
