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

        auto& skills{pinned_char->skills};
        auto views{skills.get_all_views()};
        for (int32 i{0}; i < FPlayerSkills::N_SKILLS; i++) {
            skills_values[i]->SetText(FText::AsNumber(views[i].skill.get_skill()));
        }
    } else {
        log_warning(TEXT("Couldn't pin character."));
    }
}
void UStatsMenuWidget::set_character(AMyCharacter& my_char) {
    character = &my_char;
}

void UStatsMenuWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    skills_grid_2->SetColumnFill(0, 2.0f);
    skills_grid_2->SetColumnFill(1, 1.0f);
    for (auto skill : ml::EPlayerSkillName_values) {
        auto const i{std::to_underlying(skill)};

        auto* label_tb{NewObject<UTextBlock>(this)};
        label_tb->SetText(ml::get_display_text(skill));
        skills_grid_2->AddChildToGrid(label_tb, i, 0);
        skills_labels.Add(label_tb);

        auto* value_tb{NewObject<UTextBlock>(this)};
        value_tb->SetText(FText::FromString(TEXT("???")));
        value_tb->SetJustification(ETextJustify::Right);
        skills_grid_2->AddChildToGrid(value_tb, i, 1);
        skills_values.Add(value_tb);
    }
}
void UStatsMenuWidget::NativeConstruct() {
    Super::NativeConstruct();
}
