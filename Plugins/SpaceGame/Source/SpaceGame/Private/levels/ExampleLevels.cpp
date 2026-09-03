#include <SpaceGame/levels/ExampleLevels.h>

namespace ml::example_levels {
auto make_native_example() -> FLevelDefinition {
    FLevelBuilder builder;
    builder.set_metadata(FLevelMetadata{
        .title = TEXT("Native Example"),
        .description = TEXT("A two-team level used by native tests."),
    });
    builder.add_team(FTeamDefinition{.id = level_teams::blue});
    builder.add_team(FTeamDefinition{.id = level_teams::red});
    auto const player_id{builder.add_entity(FEntitySpawnDefinition{
        .id = FLevelEntityId{FName{TEXT("player")}},
        .archetype = level_archetypes::player_fighter,
        .team = level_teams::blue,
        .position = FVector{0.0, -25000.0, 1000.0},
        .rotation = FRotator{0.0, 90.0, 0.0},
    })};
    builder.set_player_entity(player_id);
    builder.add_entity(FEntitySpawnDefinition{
        .id = FLevelEntityId{FName{TEXT("blue-capital")}},
        .archetype = level_archetypes::capital_ship,
        .team = level_teams::blue,
        .position = FVector{-40000.0, 0.0, 0.0},
    });
    builder.add_entity(FEntitySpawnDefinition{
        .id = FLevelEntityId{FName{TEXT("red-capital")}},
        .archetype = level_archetypes::capital_ship,
        .team = level_teams::red,
        .position = FVector{40000.0, 0.0, 0.0},
    });
    builder.add_entity(FEntitySpawnDefinition{
        .id = FLevelEntityId{FName{TEXT("red-turret")}},
        .archetype = level_archetypes::static_turret,
        .team = level_teams::red,
        .position = FVector{30000.0, 15000.0, 0.0},
        .rotation = FRotator{0.0, 180.0, 0.0},
    });
    return builder.finish();
}
}
