#include "SpaceGame/ui/save_game/LevelOutcomeRowWidget.h"

#include "SpaceGame/persistence/SaveGameBrowser.h"
#include "SpaceGame/support/logging/SandboxLogCategories.h"

#include <Components/Button.h>
#include <Components/TextBlock.h>

namespace ml::ioj {
namespace {
FLinearColor const selected_colour{0.16f, 0.38f, 0.55f, 1.f};
FLinearColor const inactive_colour{0.12f, 0.14f, 0.16f, 1.f};
}

void ULevelOutcomeRowWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    if (!IsValid(row_button) || !IsValid(display_name_text)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("ULevelOutcomeRowWidget::NativeOnInitialized: One or more bound widgets "
                    "are invalid."));
        return;
    }

    row_button->OnClicked.AddDynamic(this, &ThisClass::handle_clicked);
}

void ULevelOutcomeRowWidget::set_outcome(FLevelOutcomeSummary const& outcome) {
    if (!IsValid(display_name_text)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("ULevelOutcomeRowWidget::set_outcome: Display name text is invalid."));
        return;
    }

    outcome_id_ = outcome.outcome_id;
    display_name_text->SetText(FText::FromString(outcome.display_name));
}

void ULevelOutcomeRowWidget::set_selected(bool const is_selected) {
    if (!IsValid(row_button)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("ULevelOutcomeRowWidget::set_selected: Row button is invalid."));
        return;
    }

    row_button->SetBackgroundColor(is_selected ? selected_colour : inactive_colour);
}

void ULevelOutcomeRowWidget::handle_clicked() {
    selected.Broadcast(outcome_id_);
}
}
