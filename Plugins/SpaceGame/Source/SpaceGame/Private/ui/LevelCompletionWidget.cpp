#include "SpaceGame/ui/LevelCompletionWidget.h"

#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/ui/common/MenuButtonWidget.h"

#include <Components/TextBlock.h>
#include <Components/VerticalBox.h>

namespace ml::ioj {
void ULevelCompletionWidget::prepare_for_open(FString level_display_name) {
    action_requested_ = false;
    if (!IsValid(level_name_text)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("ULevelCompletionWidget::prepare_for_open: Level name text is invalid."));
        return;
    }
    level_name_text->SetText(FText::FromString(MoveTemp(level_display_name)));
}

void ULevelCompletionWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    if (!IsValid(level_name_text) || !IsValid(statistics_container) ||
        !IsValid(return_to_level_select_button) || !IsValid(keep_playing_button)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("ULevelCompletionWidget::NativeOnInitialized: One or more bound widgets are "
                    "invalid."));
        return;
    }

    return_to_level_select_button->OnClicked().AddUObject(
        this, &ThisClass::handle_return_to_level_select);
    keep_playing_button->OnClicked().AddUObject(this, &ThisClass::handle_keep_playing);
    return_to_level_select_button->SetNavigationRuleExplicit(EUINavigation::Down,
                                                             keep_playing_button);
    keep_playing_button->SetNavigationRuleExplicit(EUINavigation::Up,
                                                   return_to_level_select_button);
}

auto ULevelCompletionWidget::NativeGetDesiredFocusTarget() const -> UWidget* {
    return return_to_level_select_button;
}

auto ULevelCompletionWidget::NativeOnHandleBackAction() -> bool {
    return true;
}

void ULevelCompletionWidget::handle_return_to_level_select() {
    if (action_requested_) {
        return;
    }
    action_requested_ = true;
    return_to_level_select_requested.Broadcast();
}

void ULevelCompletionWidget::handle_keep_playing() {
    if (action_requested_) {
        return;
    }
    action_requested_ = true;
    DeactivateWidget();
}
}
