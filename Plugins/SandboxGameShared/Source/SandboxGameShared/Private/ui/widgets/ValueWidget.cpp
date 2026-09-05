#include "SandboxGameShared/ui/widgets/ValueWidget.h"

bool UValueWidget::Initialize() {
    if (!Super::Initialize()) {
        return false;
    }

    update_format_spec_text();
    return true;
}
void UValueWidget::NativePreConstruct() {
    Super::NativePreConstruct();

    update_format_spec_text();
}
void UValueWidget::NativeConstruct() {
    Super::NativeConstruct();

    update_format_spec_text();
}
void UValueWidget::update_format_spec_text() {
    format_spec_text = FText::FromName(format_spec);
    if (value_text) {
        if (text_style_) {
            auto const& style{text_style_.GetValue()};
            value_text->SetFont(style.Font);
            value_text->SetColorAndOpacity(style.ColorAndOpacity);
            value_text->SetShadowOffset(style.ShadowOffset);
            value_text->SetShadowColorAndOpacity(style.ShadowColorAndOpacity);
            value_text->SetStrikeBrush(style.StrikeBrush);
            value_text->SetTextTransformPolicy(style.TransformPolicy);
            value_text->SetTextOverflowPolicy(style.OverflowPolicy);
        } else {
            auto font{value_text->GetFont()};
            font.Size = font_size;
            value_text->SetFont(font);
        }

        if (value_text->GetText().IsEmpty()) {
            value_text->SetText(format_spec_text);
        }
    }
}

void UValueWidget::set_format_spec(FName const new_format_spec) {
    format_spec = new_format_spec;
    update_format_spec_text();
}

void UValueWidget::set_font_size(int32 const new_font_size) {
    font_size = new_font_size;
    update_format_spec_text();
}

void UValueWidget::set_text_style(FTextBlockStyle const& style) {
    text_style_ = style;
    update_format_spec_text();
}
