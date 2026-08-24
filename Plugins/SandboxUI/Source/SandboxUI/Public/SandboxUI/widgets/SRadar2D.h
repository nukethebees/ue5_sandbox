#pragma once

#include "SandboxCore/soa_vectors_2f.h"

#include "Styling/SlateBrush.h"
#include "UObject/GCObject.h"
#include "Widgets/SLeafWidget.h"

struct SANDBOXUI_API FRadar2DContactStyle {
    FRadar2DContactStyle();

    FSlateBrush brush;
    FLinearColor tint{FLinearColor::White};
    FVector2f rendered_size{6.0f, 6.0f};
};

struct SANDBOXUI_API FRadar2DStyleBucket {
    FRadar2DContactStyle style;
    FVectors2f positions;
};

struct SANDBOXUI_API FRadar2DPresentation {
    FRadar2DPresentation();

    FSlateBrush background_brush;
    FLinearColor background_tint{0.02f, 0.025f, 0.035f, 1.0f};
    FVector2f desired_size{256.0f, 256.0f};
};

struct SANDBOXUI_API FRadar2DLayout {
    FVector2f origin{FVector2f::ZeroVector};
    FVector2f size{FVector2f::ZeroVector};
    FVector2f centre{FVector2f::ZeroVector};
    float pixels_per_unit{0.0f};
};

SANDBOXUI_API auto make_radar_2d_layout(FVector2f widget_size, float range) -> FRadar2DLayout;
SANDBOXUI_API auto radar_to_local(FVector2f radar_position, FRadar2DLayout const& layout)
    -> FVector2f;

class SANDBOXUI_API SRadar2D
    : public SLeafWidget
    , public FGCObject {
  public:
    SLATE_BEGIN_ARGS(SRadar2D)
        : _Range{1.0f} {}
    SLATE_ARGUMENT(float, Range)
    SLATE_ARGUMENT(FRadar2DPresentation, Presentation)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);

    [[nodiscard]] bool set_range(float range);
    [[nodiscard]] bool set_presentation(FRadar2DPresentation presentation);
    [[nodiscard]] bool set_buckets(TArray<FRadar2DStyleBucket> buckets);
    [[nodiscard]] auto add_style(FRadar2DContactStyle style) -> int32;
    [[nodiscard]] bool set_style(int32 style_index, FRadar2DContactStyle style);
    [[nodiscard]] bool set_positions(int32 style_index, FVectors2f positions);
    [[nodiscard]] bool clear_positions(int32 style_index);
    void clear_positions();
    void clear_styles();

    auto get_range() const noexcept -> float { return range_; }
    auto get_presentation() const noexcept -> FRadar2DPresentation const& { return presentation_; }
    auto get_buckets() const noexcept -> TConstArrayView<FRadar2DStyleBucket> { return buckets_; }

    FVector2D ComputeDesiredSize(float layout_scale_multiplier) const override;
    int32 OnPaint(FPaintArgs const& args,
                  FGeometry const& allotted_geometry,
                  FSlateRect const& culling_rect,
                  FSlateWindowElementList& out_draw_elements,
                  int32 layer_id,
                  FWidgetStyle const& widget_style,
                  bool parent_enabled) const override;

    void AddReferencedObjects(FReferenceCollector& collector) override;
    FString GetReferencerName() const override;
  private:
    static bool is_valid_style(FRadar2DContactStyle const& style);
    static bool is_valid_presentation(FRadar2DPresentation const& presentation);
    static bool has_valid_array_sizes(FVectors2f const& positions);

    FRadar2DPresentation presentation_;
    TArray<FRadar2DStyleBucket> buckets_;
    float range_{1.0f};
};
