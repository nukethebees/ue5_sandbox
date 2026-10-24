#include <SpaceGame/levels/ExampleLevels.h>

namespace ioj::example_levels {
auto make_example_draft() -> FLevelDraft {
    FLevelBuilder builder;
    builder.set_metadata(FLevelMetadata{
        .id = FLevelId{FName{TEXT("native-example")}},
        .title = TEXT("Native Example"),
        .description = TEXT("A two-team level used by native tests."),
    });
    auto const player_id{builder.add_entity(FEntitySpawnDefinition{
        .id = FLevelEntityId{FName{TEXT("player")}},
        .archetype = level_archetypes::player_fighter,
        .team = Team::Blue,
        .position = FVector{0.0, -25000.0, 1000.0},
        .rotation = FRotator{0.0, 90.0, 0.0},
    })};
    builder.set_player_entity(player_id);
    builder.add_entity(FEntitySpawnDefinition{
        .id = FLevelEntityId{FName{TEXT("blue-capital")}},
        .archetype = level_archetypes::capital_ship,
        .team = Team::Blue,
        .position = FVector{-40000.0, 0.0, 0.0},
    });
    builder.add_entity(FEntitySpawnDefinition{
        .id = FLevelEntityId{FName{TEXT("red-capital")}},
        .archetype = level_archetypes::capital_ship,
        .team = Team::Red,
        .position = FVector{40000.0, 0.0, 0.0},
    });
    builder.add_entity(FEntitySpawnDefinition{
        .id = FLevelEntityId{FName{TEXT("red-turret")}},
        .archetype = level_archetypes::static_turret,
        .team = Team::Red,
        .position = FVector{30000.0, 15000.0, 0.0},
        .rotation = FRotator{0.0, 180.0, 0.0},
    });
    builder.set_mission(levels::MissionDefinition{
        .objectives = {{.id = ioj::levels::ObjectiveName{"destroy-capital"},
                        .title = "Destroy the enemy capital",
                        .team = Team::Blue,
                        .expressions = {{ioj::levels::EntityDestroyedExpression{
                                            .entity = levels::EntityId{"red-capital"}}},
                                        {ioj::levels::EntityDestroyedExpression{
                                            .entity = levels::EntityId{"blue-capital"}}},
                                        {ioj::levels::EntityKillsReachedExpression{
                                            .entities = {levels::EntityId{"player"},
                                                         levels::EntityId{"blue-capital"}},
                                            .kills = 2}},
                                        {ioj::levels::AllExpression{.children = {0, 2}}}},
                        .roots =
                            ioj::levels::ObjectiveRoots{std::array<ioj::levels::ExpressionIndex, 3>{
                                3, 1, levels::absent_expression}}}},
        .team_results = {
            {.team = Team::Blue,
             .when = {ioj::levels::ObjectiveName{"destroy-capital"}, ObjectiveOutcome::Passed},
             .result = TeamOutcome::Victory},
            {.team = Team::Blue,
             .when = {ioj::levels::ObjectiveName{"destroy-capital"}, ObjectiveOutcome::Failed},
             .result = TeamOutcome::Defeat}}});
    return builder.finish();
}
}
