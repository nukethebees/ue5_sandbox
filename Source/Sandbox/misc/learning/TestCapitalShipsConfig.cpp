#include "TestCapitalShipsConfig.h"

#if WITH_EDITOR
void UTestCapitalShipsConfig::PostEditChangeProperty(FPropertyChangedEvent& event) {
    Super::PostEditChangeProperty(event);

    synchronise_data();
}
void UTestCapitalShipsConfig::PostLoad() {
    Super::PostLoad();

    synchronise_data();
}
#endif

void UTestCapitalShipsConfig::synchronise_data() {
    if (fighter_spawn_slots < 0) { fighter_spawn_slots = 0; }

    fighter_spawn_slots_relative_transforms.SetNum(fighter_spawn_slots);
}
