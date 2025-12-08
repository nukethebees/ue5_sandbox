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
        auto* label_tb{NewObject<UTextBlock>(this)};
        label_tb->SetText(ml::get_display_text(skill));
        skills_grid_2->AddChildToGrid(label_tb, std::to_underlying(skill), 0);
        skills_labels.Add(label_tb);

        auto* value_tb{NewObject<UTextBlock>(this)};
        value_tb->SetText(FText::FromString(TEXT("???")));
        skills_grid_2->AddChildToGrid(value_tb, std::to_underlying(skill), 1);
        skills_values.Add(value_tb);
    }
}
void UStatsMenuWidget::NativeConstruct() {
    Super::NativeConstruct();
}
