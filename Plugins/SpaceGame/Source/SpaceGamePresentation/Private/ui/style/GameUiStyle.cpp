#include "SpaceGamePresentation/ui/style/GameUiStyle.h"

#include <Components/TextBlock.h>
#include <Widgets/Layout/SBorder.h>

namespace ml::ioj {
auto FGameUiStyle::text(EGameTextStyle const role) const -> FTextBlockStyle const& {
    return text_styles_[role];
}

auto FGameUiStyle::button(EGameButtonStyle const role) const
    -> FGameButtonPresentationStyle const& {
    return button_styles_[role];
}

auto FGameUiStyle::panel() const -> FGamePanelStyle const& {
    return panel_;
}

auto FGameUiStyle::settings() const -> FSettingsStyle const& {
    return settings_;
}

auto FGameUiStyle::palette() const -> FGameUiPalette const& {
    return palette_;
}

auto FGameUiStyle::chrome() const -> FGameUiChromeStyle const& {
    return chrome_;
}

auto FGameUiStyle::icon(EGameUiIcon const role) const -> FSlateBrush const& {
    return icons_[role];
}

auto FGameUiStyle::hud() const -> FGameHudStyle const& {
    return hud_;
}

void apply_text_style(UTextBlock& text, FTextBlockStyle const& style) {
    text.SetFont(style.Font);
    text.SetColorAndOpacity(style.ColorAndOpacity);
    text.SetShadowOffset(style.ShadowOffset);
    text.SetShadowColorAndOpacity(style.ShadowColorAndOpacity);
    text.SetStrikeBrush(style.StrikeBrush);
    text.SetTextTransformPolicy(style.TransformPolicy);
    text.SetTextOverflowPolicy(style.OverflowPolicy);
}

auto make_hud_panel(FGameHudStyle const& style, TSharedRef<SWidget> content)
    -> TSharedRef<SWidget> {
    return SNew(SBorder)
        .BorderImage(&style.border)
        .Padding(FMargin{1.0f})[SNew(SBorder)
                                    .BorderImage(&style.panel_background)
                                    .Padding(style.panel_padding)[MoveTemp(content)]];
}
}
