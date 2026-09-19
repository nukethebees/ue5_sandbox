#include "SpaceGamePresentation/presentation/widgets/Vector2DWidget.h"

#include "SpaceGamePresentation/ui/style/GameUiStyle.h"

#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

void UVector2DWidget::NativePreConstruct() {
    Super::NativePreConstruct();
    rebuild_widget_tree();
    update_widgets();
}

void UVector2DWidget::update(FVector2D const value) {
    value_ = value;
    update_widgets();
}

void UVector2DWidget::apply_hud_style(ml::ioj::FGameHudStyle const& style) {
    hud_style_ = style;
    update_widgets();
}

void UVector2DWidget::set_font_size(int32 const new_font_size) {
    font_size = new_font_size;
    update_widgets();
}

void UVector2DWidget::set_label(FText new_label) {
    name = MoveTemp(new_label);
    update_widgets();
}

void UVector2DWidget::rebuild_widget_tree() {
    if (!WidgetTree) {
        return;
    }

    if (!root_size_box_) {
        root_size_box_ =
            WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("root_size_box"));
        root_size_box_->SetMinDesiredWidth(160.0f);
        root_size_box_->SetMinDesiredHeight(120.0f);
        WidgetTree->RootWidget = root_size_box_;
    }
    if (!canvas_panel) {
        canvas_panel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
                                                                 TEXT("canvas_panel"));
        root_size_box_->SetContent(canvas_panel);
    }
    if (!background_widget) {
        background_widget =
            WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("background_widget"));
        auto* const slot{canvas_panel->AddChildToCanvas(background_widget)};
        slot->SetAnchors(FAnchors{0.0f, 0.0f, 1.0f, 1.0f});
        slot->SetOffsets(FMargin{0.0f});
    }
    if (!cursor_widget) {
        cursor_widget =
            WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("cursor_widget"));
        auto* const slot{canvas_panel->AddChildToCanvas(cursor_widget)};
        slot->SetSize(FVector2D{12.0f, 12.0f});
        slot->SetAlignment(FVector2D{0.5f, 0.5f});
        slot->SetZOrder(1);
    }
    if (!name_text) {
        name_text =
            WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("name_text"));
        auto* const slot{canvas_panel->AddChildToCanvas(name_text)};
        slot->SetPosition(FVector2D{4.0f, 2.0f});
        slot->SetAutoSize(true);
        slot->SetZOrder(2);
    }
    if (!value_text) {
        value_text =
            WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("value_text"));
        auto* const slot{canvas_panel->AddChildToCanvas(value_text)};
        slot->SetAnchors(FAnchors{0.0f, 1.0f});
        slot->SetAlignment(FVector2D{0.0f, 1.0f});
        slot->SetPosition(FVector2D{4.0f, -2.0f});
        slot->SetAutoSize(true);
        slot->SetZOrder(2);
    }
}

void UVector2DWidget::update_widgets() {
    if (name_text) {
        name_text->SetText(name);
    }
    if (value_text) {
        value_text->SetVisibility(show_value ? ESlateVisibility::Visible
                                             : ESlateVisibility::Collapsed);
        auto number_format{FNumberFormattingOptions{}};
        number_format.SetMinimumFractionalDigits(1);
        number_format.SetMaximumFractionalDigits(1);

        auto const display_value{FText::Format(INVTEXT("{0}, {1} ({2})"),
                                               FText::AsNumber(value_.X, &number_format),
                                               FText::AsNumber(value_.Y, &number_format),
                                               FText::AsNumber(value_.Size(), &number_format))};
        value_text->SetText(display_value);
    }

    if (!canvas_panel || !background_widget || !cursor_widget) {
        return;
    }

    auto* const cursor_slot{Cast<UCanvasPanelSlot>(cursor_widget->Slot)};
    if (!cursor_slot) {
        return;
    }

    auto const cursor_position{FVector2D{
        FMath::GetMappedRangeValueClamped(FVector2D{-1.f, 1.f}, FVector2D{0.f, 1.f}, value_.X),
        FMath::GetMappedRangeValueClamped(FVector2D{-1.f, 1.f}, FVector2D{1.f, 0.f}, value_.Y),
    }};

    cursor_slot->SetAnchors(FAnchors{
        static_cast<float>(cursor_position.X),
        static_cast<float>(cursor_position.Y),
    });
    cursor_slot->SetAlignment(FVector2D{0.5, 0.5});
    cursor_slot->SetPosition(FVector2D::ZeroVector);
    if (hud_style_) {
        auto const& style{hud_style_.GetValue()};
        background_widget->SetBrush(style.control_background);
        cursor_widget->SetBrushColor(style.energy);
        if (name_text) {
            ml::ioj::apply_text_style(*name_text, style.caption_text);
        }
        if (value_text) {
            ml::ioj::apply_text_style(*value_text, style.secondary_text);
        }
    }
    if (name_text) {
        auto font{name_text->GetFont()};
        font.Size = font_size;
        name_text->SetFont(font);
    }
    if (value_text) {
        auto font{value_text->GetFont()};
        font.Size = font_size;
        value_text->SetFont(font);
    }
}
