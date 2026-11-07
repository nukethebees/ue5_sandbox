#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"

#include "SandboxEditor/levels/S7LevelAuthoringActors.h"
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <Engine/World.h>

AS7LevelAuthoringDocument::AS7LevelAuthoringDocument() {
    SetActorHiddenInGame(true);
    SetCanBeDamaged(false);
}

void AS7LevelAuthoringDocument::PostRegisterAllComponents() {
    Super::PostRegisterAllComponents();
    auto const* world{GetWorld()};
    if (IsValid(world) && world->WorldType == EWorldType::Editor) {
        refresh_entity_previews();
    }
}

void AS7LevelAuthoringDocument::PostEditChangeProperty(FPropertyChangedEvent& event) {
    Super::PostEditChangeProperty(event);
    refresh_entity_previews();
}

void AS7LevelAuthoringDocument::refresh_entity_previews() {
    if (!IsValid(level_config)) {
        return;
    }
    for (auto const& binding : entities) {
        auto* actor{binding.actor.Get()};
        if (!IsValid(actor)) {
            continue;
        }
        auto const resolved{ioj::editor::resolve_s7_level_actor(*actor)};
        if (resolved) {
            ioj::editor::configure_s7_level_actor(*actor,
                                                  resolved->archetype,
                                                  binding.team,
                                                  *level_config,
                                                  actor->GetActorTransform(),
                                                  FName{binding.id});
        }
    }
}
