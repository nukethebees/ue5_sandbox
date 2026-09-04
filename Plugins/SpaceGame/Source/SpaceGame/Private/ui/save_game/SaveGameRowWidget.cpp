#include "SpaceGame/ui/save_game/SaveGameRowWidget.h"

#include "SpaceGame/persistence/SaveGameBrowser.h"
#include "SpaceGame/support/logging/SandboxLogCategories.h"

#include <Components/Button.h>
#include <Components/TextBlock.h>

namespace ml::ioj {
namespace save_game_row_widget {
FLinearColor const selected_colour{0.16f, 0.38f, 0.55f, 1.f};
FLinearColor const inactive_colour{0.12f, 0.14f, 0.16f, 1.f};
}

void USaveGameRowWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    if (!IsValid(row_button) || !IsValid(display_name_text)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("USaveGameRowWidget::NativeOnInitialized: One or more bound widgets are "
                    "invalid."));
        return;
    }

    row_button->OnClicked.AddDynamic(this, &ThisClass::handle_clicked);
}

void USaveGameRowWidget::set_summary(FSaveProfileSummary const& summary) {
    if (!IsValid(display_name_text)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("USaveGameRowWidget::set_summary: Display name text is invalid."));
        return;
    }

    profile_id_ = summary.profile_id;
    display_name_text->SetText(FText::FromString(
        summary.active ? FString::Printf(TEXT("%s  [Active]"), *summary.display_name)
                       : summary.display_name));
}

void USaveGameRowWidget::set_selected(bool const is_selected) {
    if (!IsValid(row_button)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("USaveGameRowWidget::set_selected: Row button is invalid."));
        return;
    }

    row_button->SetBackgroundColor(is_selected ? save_game_row_widget::selected_colour
                                               : save_game_row_widget::inactive_colour);
}

void USaveGameRowWidget::focus_row() {
    if (!IsValid(row_button)) {
        UE_LOG(
            LogSandboxUI, Warning, TEXT("USaveGameRowWidget::focus_row: Row button is invalid."));
        return;
    }

    row_button->SetKeyboardFocus();
}

auto USaveGameRowWidget::get_focus_target() const -> UWidget* {
    return row_button;
}

void USaveGameRowWidget::handle_clicked() {
    selected.Broadcast(profile_id_);
}
}
