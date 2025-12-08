#include "Sandbox/ui/in_game_menu/widgets/umg/StatsMenuWidget.h"

#include <utility>

#include "Components/GridPanel.h"
#include "Components/TextBlock.h"

#include "Sandbox/players/playable/characters/MyCharacter.h"
#include "Sandbox/players/playable/data/PlayerSkills.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UStatsMenuWidget::on_widget_selected() {
    RETURN_IF_INVALID(character);

    if (auto pinned_char{character.Pin()}) {
        strength_value->SetText(FText::AsNumber(pinned_char->attributes.strength));
        endurance_value->SetText(FText::AsNumber(pinned_char->attributes.endurance));
        agility_value->SetText(FText::AsNumber(pinned_char->attributes.agility));
        cyber_value->SetText(FText::AsNumber(pinned_char->attributes.cyber));
        psi_value->SetText(FText::AsNumber(pinned_char->attributes.psi));

        hacking_value->SetText(FText::AsNumber(pinned_char->tech_skills.hacking));
    } else {
        log_warning(TEXT("Couldn't pin character."));
    }
}
void UStatsMenuWidget::set_character(AMyCharacter& my_char) {
    character = &my_char;
}

void UStatsMenuWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    for (auto skill : ml::EPlayerSkillName_values) {
        auto* tb{NewObject<UTextBlock>(this)};
        tb->SetText(ml::get_display_text(skill));
        skills_grid_2->AddChildToGrid(tb, std::to_underlying(skill), 0);
    }
}
void UStatsMenuWidget::NativeConstruct() {
    Super::NativeConstruct();
}
