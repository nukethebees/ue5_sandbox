#include "Sandbox/utilities/SandboxStyle.h"

#include "Sandbox/utilities/ui.h"
#include "Styling/SlateStyleRegistry.h"
#include "Fonts/CompositeFont.h"

namespace ml {

TUniquePtr<FSlateStyleSet> SandboxStyle::style_set_{};

void SandboxStyle::initialize() {
    if (style_set_.IsValid()) {
        return; // Already initialized
    }

    style_set_ = MakeUnique<FSlateStyleSet>(get_style_set_name());
    initialize_text_styles();
    FSlateStyleRegistry::RegisterSlateStyle(*style_set_.Get());
}

void SandboxStyle::shutdown() {
    if (style_set_.IsValid()) {
        FSlateStyleRegistry::UnRegisterSlateStyle(*style_set_.Get());
        style_set_.Reset();
    }
}

FName SandboxStyle::get_style_set_name() {
    static FName style_name{TEXT("SandboxStyle")};
    return style_name;
}

void SandboxStyle::initialize_text_styles() {
    auto const widget_font_size{ml::scale_font_to_umg(32)};
    FTextBlockStyle widget_style{};
    widget_style.SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Bold", widget_font_size)));
    widget_style.SetColorAndOpacity(FLinearColor::White);
    style_set_->Set("Sandbox.Text.Widget", widget_style);

    // Body text style
    auto const body_font_size{ml::scale_font_to_umg(16)};
    FTextBlockStyle body_style{};
    body_style.SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Regular", body_font_size)));
    body_style.SetColorAndOpacity(FLinearColor::White);
    style_set_->Set("Sandbox.Text.Body", body_style);

    // Label text style
    auto const label_font_size{ml::scale_font_to_umg(14)};
    FTextBlockStyle label_style{};
    label_style.SetFont(
        FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Regular", label_font_size)));
    label_style.SetColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f));
    style_set_->Set("Sandbox.Text.Label", label_style);
}

}
