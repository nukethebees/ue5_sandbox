#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"

#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

AS7LevelAuthoringDocument::AS7LevelAuthoringDocument() {
    SetActorHiddenInGame(true);
    SetCanBeDamaged(false);
}

void AS7LevelAuthoringDocument::PostActorCreated() {
    Super::PostActorCreated();
    grid_override_schema_version = 1;
}

void AS7LevelAuthoringDocument::PostRegisterAllComponents() {
    Super::PostRegisterAllComponents();
    migrate_collision_grid_overrides();
}

auto AS7LevelAuthoringDocument::collision_grid_overrides() const
    -> ml::FLevelCollisionGridDefinition {
    ml::FLevelCollisionGridDefinition result;
    auto const* const config{level_config.Get()};
    auto const legacy{grid_override_schema_version == 0 && IsValid(config)};
    if (level_size != FVector3f::ZeroVector &&
        (!legacy || level_size != config->collision_grid.grid_size)) {
        result.level_size = level_size;
    }
    if (grid_cell_size != FVector3f::ZeroVector &&
        (!legacy || grid_cell_size != config->collision_grid.cell_size)) {
        result.cell_size = grid_cell_size;
    }
    return result;
}

auto AS7LevelAuthoringDocument::resolve_collision_grid(FCollisionGridConfig const& source) const
    -> FCollisionGridConfig {
    auto const overrides{collision_grid_overrides()};
    return source.with_dimension_overrides(overrides.level_size, overrides.cell_size);
}

void AS7LevelAuthoringDocument::migrate_collision_grid_overrides() {
    if (grid_override_schema_version != 0 || !IsValid(level_config)) {
        return;
    }

    // Matching legacy copies become inherited. Preserve differing values because their original
    // intent cannot be recovered from the old document.
    auto const overrides{collision_grid_overrides()};
    Modify();
    level_size = overrides.level_size.Get(FVector3f::ZeroVector);
    grid_cell_size = overrides.cell_size.Get(FVector3f::ZeroVector);
    grid_override_schema_version = 1;
}
