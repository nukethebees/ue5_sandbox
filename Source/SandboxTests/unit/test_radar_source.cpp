#include <SpaceGame/presentation/RadarSource.h>

#include <CQTest.h>

namespace ml::test_radar_source {
auto make_view(entity_registry::EntityData const& entities)
    -> entity_registry::EntityData::ConstView {
    return {.locations = {entities.locations.xs, entities.locations.ys, entities.locations.zs},
            .velocities = {entities.velocities.xs, entities.velocities.ys, entities.velocities.zs},
            .radii = entities.radii,
            .healths = entities.healths,
            .teams = entities.teams,
            .entity_types = entities.entity_types,
            .alive = entities.alive};
}

void add_entity(entity_registry::EntityData& entities,
                FVector3f const location,
                ETestTeam const team,
                ETestEntityType const type,
                bool const alive = true) {
    entities.locations.add(location);
    entities.velocities.add(FVector3f::ZeroVector);
    entities.radii.Add(100.0f);
    entities.healths.Add(100);
    entities.teams.Add(team);
    entities.entity_types.Add(type);
    entities.alive.Add(alive ? 1 : 0);
}

auto glyph(FRadarInstance const& instance) -> ERadarGlyph {
    return static_cast<ERadarGlyph>(instance.packed_glyph_and_flags & 0xffu);
}

auto flags(FRadarInstance const& instance) -> ERadarContactFlags {
    return static_cast<ERadarContactFlags>(instance.packed_glyph_and_flags >> 8u);
}

auto nearly_equal(FVector3f const lhs, FVector3f const rhs) -> bool {
    return lhs.Equals(rhs, 0.0001f);
}
} // namespace ml::test_radar_source

TEST_CLASS(RadarSource, "Sandbox.UnitTests")
{
    TEST_METHOD(FiltersMapsAndOrdersContacts)
    {
        ml::entity_registry::EntityData entities;
        ml::test_radar_source::add_entity(
            entities, FVector3f::ZeroVector, ETestTeam::Blue, ETestEntityType::PlayerShip);
        ml::test_radar_source::add_entity(
            entities, {50.0f, 0.0f, 0.0f}, ETestTeam::Red, ETestEntityType::CapitalShipFighter);
        ml::test_radar_source::add_entity(
            entities, {0.0f, 50.0f, 25.0f}, ETestTeam::Green, ETestEntityType::CapitalShip);
        ml::test_radar_source::add_entity(
            entities, {-50.0f, 0.0f, 0.0f}, ETestTeam::Yellow, ETestEntityType::Turret);
        ml::test_radar_source::add_entity(
            entities, {0.0f, -50.0f, 0.0f}, ETestTeam::White, ETestEntityType::TubeSpinner);
        ml::test_radar_source::add_entity(
            entities, {100.0f, 0.0f, 0.0f}, ETestTeam::Red, ETestEntityType::Turret);
        ml::test_radar_source::add_entity(entities,
                                          {25.0f, 0.0f, 0.0f},
                                          ETestTeam::Red,
                                          ETestEntityType::CapitalShipFighter,
                                          false);
        ml::test_radar_source::add_entity(
            entities, {101.0f, 0.0f, 0.0f}, ETestTeam::Red, ETestEntityType::CapitalShip);

        TArray<int32> generations;
        generations.Init(7, entities.alive.Num());
        TArray<EEntityOverlayObjectiveRole> objective_roles;
        objective_roles.Init(EEntityOverlayObjectiveRole::None, entities.alive.Num());
        objective_roles[3] = EEntityOverlayObjectiveRole::Destroy;
        objective_roles[7] = EEntityOverlayObjectiveRole::Defend;

        FRadarTeamColours colours;
        colours.fighter[ETestTeam::Red] = FLinearColor{1.0f, 0.0f, 0.0f, 1.0f};
        colours.capital_ship[ETestTeam::Green] = FLinearColor{0.0f, 1.0f, 0.0f, 1.0f};
        colours.turret[ETestTeam::Yellow] = FLinearColor{1.0f, 1.0f, 0.0f, 1.0f};

        TArray<FRadarInstance> instances;
        auto const result{collect_radar_instances(ml::test_radar_source::make_view(entities),
                                                  generations,
                                                  objective_roles,
                                                  colours,
                                                  FTransform::Identity,
                                                  {0, 7},
                                                  {2, 7},
                                                  100.0f,
                                                  instances)};

        TestRunner->TestEqual(
            TEXT("All alive non-player rows are candidates"), result.candidate_count, 6);
        TestRunner->TestEqual(
            TEXT("Only in-range contacts and the player are visible"), result.visible_count, 5);
        TestRunner->TestEqual(TEXT("Normal fighter is drawn first"),
                              ml::test_radar_source::glyph(instances[0]),
                              ERadarGlyph::Fighter);
        TestRunner->TestEqual(TEXT("Unsupported type maps to unknown"),
                              ml::test_radar_source::glyph(instances[1]),
                              ERadarGlyph::Unknown);
        TestRunner->TestEqual(TEXT("Selected contact follows normal contacts"),
                              ml::test_radar_source::glyph(instances[2]),
                              ERadarGlyph::CapitalShip);
        TestRunner->TestEqual(TEXT("Selection preserves its flag"),
                              ml::test_radar_source::flags(instances[2]),
                              ERadarContactFlags::Selected);
        TestRunner->TestEqual(TEXT("Objective follows normal contacts"),
                              ml::test_radar_source::glyph(instances[3]),
                              ERadarGlyph::Turret);
        TestRunner->TestEqual(TEXT("Objective preserves its role"),
                              ml::test_radar_source::flags(instances[3]),
                              ERadarContactFlags::DestroyObjective);
        TestRunner->TestEqual(TEXT("Player is always last"),
                              ml::test_radar_source::glyph(instances.Last()),
                              ERadarGlyph::Player);
        TestRunner->TestTrue(TEXT("Player is central"),
                             instances.Last().radar_position.IsNearlyZero());
        TestRunner->TestEqual(TEXT("Fighter preserves its team colour"),
                              instances[0].packed_color,
                              pack_radar_color(FLinearColor{1.0f, 0.0f, 0.0f, 1.0f}));

        auto const zero_range_result{
            collect_radar_instances(ml::test_radar_source::make_view(entities),
                                    generations,
                                    objective_roles,
                                    colours,
                                    FTransform::Identity,
                                    {0, 7},
                                    {},
                                    0.0f,
                                    instances)};
        TestRunner->TestEqual(
            TEXT("Zero range retains only the player"), zero_range_result.visible_count, 1);
    }

    TEST_METHOD(UsesYawAndPitchButRemovesRoll)
    {
        FRotator const no_roll_rotation{28.0, 37.0, 0.0};
        FVector const player_location{1200.0, -500.0, 300.0};
        FVector3f const local_contact{50.0f, 25.0f, 20.0f};
        auto const world_contact{
            FVector3f{player_location + no_roll_rotation.RotateVector(FVector{local_contact})}};

        ml::entity_registry::EntityData entities;
        ml::test_radar_source::add_entity(
            entities, FVector3f{player_location}, ETestTeam::Blue, ETestEntityType::PlayerShip);
        ml::test_radar_source::add_entity(
            entities, world_contact, ETestTeam::Red, ETestEntityType::CapitalShipFighter);
        TArray<int32> generations;
        generations.Init(3, entities.alive.Num());
        TArray<EEntityOverlayObjectiveRole> objective_roles;
        objective_roles.Init(EEntityOverlayObjectiveRole::None, entities.alive.Num());
        FRadarTeamColours colours;
        TArray<FRadarInstance> no_roll_instances;
        TArray<FRadarInstance> rolled_instances;

        static_cast<void>(collect_radar_instances(ml::test_radar_source::make_view(entities),
                                                  generations,
                                                  objective_roles,
                                                  colours,
                                                  FTransform{no_roll_rotation, player_location},
                                                  {0, 3},
                                                  {},
                                                  100.0f,
                                                  no_roll_instances));
        static_cast<void>(
            collect_radar_instances(ml::test_radar_source::make_view(entities),
                                    generations,
                                    objective_roles,
                                    colours,
                                    FTransform{FRotator{28.0, 37.0, 120.0}, player_location},
                                    {0, 3},
                                    {},
                                    100.0f,
                                    rolled_instances));

        TestRunner->TestEqual(
            TEXT("Each orientation emits contact and player"), no_roll_instances.Num(), 2);
        TestRunner->TestEqual(
            TEXT("Rolled orientation emits contact and player"), rolled_instances.Num(), 2);
        TestRunner->TestTrue(
            TEXT("Roll does not rotate the contact field"),
            ml::test_radar_source::nearly_equal(no_roll_instances[0].radar_position,
                                                rolled_instances[0].radar_position));
        TestRunner->TestTrue(
            TEXT("Yaw and pitch recover player-relative height"),
            FMath::IsNearlyEqual(no_roll_instances[0].radar_position.Z, 0.2f, 0.0001f));
        TestRunner->TestTrue(TEXT("Forward remains positive on the radar"),
                             no_roll_instances[0].radar_position.X > 0.0f);
        TestRunner->TestTrue(TEXT("Right remains positive on the radar"),
                             no_roll_instances[0].radar_position.Y > 0.0f);
    }
};
