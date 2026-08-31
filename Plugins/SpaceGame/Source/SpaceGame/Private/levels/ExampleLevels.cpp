#include <SpaceGame/levels/ExampleLevels.h>

namespace ml::example_levels {
auto make_native_example() -> FLevelDefinition {
    FLevelBuilder builder;
    builder.add_team(FTeamDefinition{.id = level_teams::blue});
    builder.add_team(FTeamDefinition{.id = level_teams::red});
    builder.set_player(FPlayerDefinition{
        .id = FLevelEntityId{FName{TEXT("player")}},
        .archetype = level_archetypes::player_fighter,
        .team = level_teams::blue,
        .position = FVector{100.0, 200.0, 300.0},
        .rotation = FRotator{0.0, 45.0, 0.0},
    });
    builder.add_entity(FEntitySpawnDefinition{
        .id = FLevelEntityId{FName{TEXT("blue-capital")}},
        .archetype = level_archetypes::capital_ship,
        .team = level_teams::blue,
        .position = FVector{1000.0, 0.0, 0.0},
    });
    builder.add_entity(FEntitySpawnDefinition{
        .id = FLevelEntityId{FName{TEXT("red-capital")}},
        .archetype = level_archetypes::capital_ship,
        .team = level_teams::red,
        .position = FVector{-1000.0, 0.0, 0.0},
    });
    builder.add_entity(FEntitySpawnDefinition{
        .id = FLevelEntityId{FName{TEXT("red-turret")}},
        .archetype = level_archetypes::static_turret,
        .team = level_teams::red,
        .position = FVector{0.0, 500.0, 0.0},
        .rotation = FRotator{0.0, 90.0, 0.0},
    });
    return builder.finish();
}
}
