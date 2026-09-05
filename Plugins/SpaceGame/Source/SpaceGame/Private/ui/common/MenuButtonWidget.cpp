#include "SpaceGame/ui/common/MenuButtonWidget.h"

#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGame/ui/style/SpaceGameUiTheme.h"

#include <Engine/GameInstance.h>
#include <Styling/CoreStyle.h>

namespace ml::ioj {
namespace {
auto make_brush(FLinearColor const& colour) -> FSlateBrush {
    FSlateBrush brush;
    brush.DrawAs = ESlateBrushDrawType::Box;
    brush.TintColor = FSlateColor{colour};
    return brush;
}
}

UMenuTextStyle::UMenuTextStyle() {
    Font = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16);
    Color = FLinearColor::White;
}

UMenuButtonStyle::UMenuButtonStyle() {
    bSingleMaterial = false;
    NormalBase = make_brush(FLinearColor{0.12f, 0.14f, 0.16f, 1.f});
    NormalHovered = make_brush(FLinearColor{0.16f, 0.38f, 0.55f, 1.f});
    NormalPressed = make_brush(FLinearColor{0.08f, 0.3f, 0.65f, 1.f});
    SelectedBase = make_brush(FLinearColor{0.08f, 0.3f, 0.65f, 1.f});
    SelectedHovered = make_brush(FLinearColor{0.12f, 0.4f, 0.78f, 1.f});
    SelectedPressed = make_brush(FLinearColor{0.06f, 0.24f, 0.52f, 1.f});
    Disabled = make_brush(FLinearColor{0.08f, 0.09f, 0.1f, 0.6f});
    ButtonPadding = FMargin{12.f, 6.f};
    MinHeight = 32;
    NormalTextStyle = UMenuTextStyle::StaticClass();
    NormalHoveredTextStyle = UMenuTextStyle::StaticClass();
    SelectedTextStyle = UMenuTextStyle::StaticClass();
    SelectedHoveredTextStyle = UMenuTextStyle::StaticClass();
    DisabledTextStyle = UMenuTextStyle::StaticClass();
}

UMenuButtonWidget::UMenuButtonWidget() {
    SetStyle(UMenuButtonStyle::StaticClass());
}

void UMenuButtonWidget::set_text(FText const& text) {
    text_ = text;
    if (IsValid(label_text)) {
        label_text->SetText(text_);
    }
}

auto UMenuButtonWidget::get_text() const -> FText {
    return text_;
}

void UMenuButtonWidget::NativePreConstruct() {
    auto style{style_override_};
    if (!style) {
        auto const* const game_instance{GetGameInstance()};
        auto const* const subsystem{
            IsValid(game_instance) ? game_instance->GetSubsystem<UGameSubsystem>() : nullptr};
        if (IsValid(subsystem)) {
            style = subsystem->get_ui_theme().get_common_button_style(style_role_);
        }
    }
    if (style) {
        SetStyle(style);
    } else {
        SetStyle(UMenuButtonStyle::StaticClass());
    }

    Super::NativePreConstruct();
    if (IsValid(label_text)) {
        label_text->SetText(text_);
    }
}

void UMenuButtonWidget::NativeOnCurrentTextStyleChanged() {
    Super::NativeOnCurrentTextStyleChanged();
    if (IsValid(label_text)) {
        label_text->SetStyle(GetCurrentTextStyleClass());
    }
}
}
