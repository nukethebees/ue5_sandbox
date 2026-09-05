#include "SpaceGame/ui/style/GameUiStyle.h"

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

auto FGameUiStyle::health_bar() const -> FProgressBarStyle const& {
    return health_bar_;
}
}
