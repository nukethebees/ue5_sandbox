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

auto linear_settings(float const maximum_range = 100.0f) -> FRadarSettings {
    FRadarSettings settings;
    settings.combat_range = maximum_range * 0.25f;
    settings.tactical_range = maximum_range * 0.5f;
    settings.maximum_range = maximum_range;
    settings.combat_display_radius = 0.25f;
    return sanitize_radar_settings(settings);
}

auto nonlinear_settings() -> FRadarSettings {
    FRadarSettings settings;
    settings.combat_range = 100.0f;
    settings.tactical_range = 400.0f;
    settings.maximum_range = 2000.0f;
    settings.combat_display_radius = 0.45f;
    return sanitize_radar_settings(settings);
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
        objective_roles[2] = EEntityOverlayObjectiveRole::Defend;
        objective_roles[3] = EEntityOverlayObjectiveRole::Destroy;
        objective_roles[7] = EEntityOverlayObjectiveRole::Defend;

        FRadarContactColours colours;
        colours.hostile = FLinearColor::Red;

        FRadarFrame frame;
        auto& instances{frame.instances};
        auto const result{collect_radar_instances(ml::test_radar_source::make_view(entities),
                                                  generations,
                                                  objective_roles,
                                                  colours,
                                                  FTransform::Identity,
                                                  {0, 7},
                                                  {2, 7},
                                                  ETestTeam::Blue,
                                                  ml::test_radar_source::linear_settings(),
                                                  frame)};

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
        TestRunner->TestEqual(TEXT("Objective follows normal contacts"),
                              ml::test_radar_source::glyph(instances[2]),
                              ERadarGlyph::Turret);
        TestRunner->TestEqual(TEXT("Objective preserves its role"),
                              ml::test_radar_source::flags(instances[2]),
                              ERadarContactFlags::DestroyObjective);
        TestRunner->TestEqual(TEXT("Selected contact follows objectives"),
                              ml::test_radar_source::glyph(instances[3]),
                              ERadarGlyph::CapitalShip);
        TestRunner->TestEqual(TEXT("Selection preserves its flag"),
                              ml::test_radar_source::flags(instances[3]),
                              ERadarContactFlags::Selected | ERadarContactFlags::DefendObjective);
        TestRunner->TestEqual(TEXT("Player is always last"),
                              ml::test_radar_source::glyph(instances.Last()),
                              ERadarGlyph::Player);
        TestRunner->TestTrue(TEXT("Player is central"),
                             instances.Last().radar_position.IsNearlyZero());
        TestRunner->TestEqual(TEXT("Hostile fighter uses the hostile colour"),
                              instances[0].packed_color,
                              0xff0000ffu);
        TestRunner->TestEqual(TEXT("Blue occupies the shader's blue channel"),
                              pack_radar_color(FLinearColor{0.0f, 0.0f, 1.0f, 1.0f}),
                              0xffff0000u);

        auto zero_range_settings{ml::test_radar_source::linear_settings()};
        zero_range_settings.maximum_range = UE_SMALL_NUMBER;
        zero_range_settings.combat_range = UE_SMALL_NUMBER;
        zero_range_settings.tactical_range = UE_SMALL_NUMBER;
        zero_range_settings.combat_display_radius = 1.0f;
        auto const zero_range_result{
            collect_radar_instances(ml::test_radar_source::make_view(entities),
                                    generations,
                                    objective_roles,
                                    colours,
                                    FTransform::Identity,
                                    {0, 7},
                                    {},
                                    ETestTeam::Blue,
                                    zero_range_settings,
                                    frame)};
        TestRunner->TestEqual(
            TEXT("Zero range retains only the player"), zero_range_result.visible_count, 1);
    }

    TEST_METHOD(UsesPlayerRelativeContactColours)
    {
        ml::entity_registry::EntityData entities;
        ml::test_radar_source::add_entity(
            entities, FVector3f::ZeroVector, ETestTeam::Blue, ETestEntityType::PlayerShip);
        ml::test_radar_source::add_entity(
            entities, {10.0f, 0.0f, 0.0f}, ETestTeam::Blue, ETestEntityType::CapitalShipFighter);
        ml::test_radar_source::add_entity(
            entities, {20.0f, 0.0f, 0.0f}, ETestTeam::Red, ETestEntityType::CapitalShip);
        ml::test_radar_source::add_entity(
            entities, {30.0f, 0.0f, 0.0f}, ETestTeam::White, ETestEntityType::Turret);

        TArray<int32> generations;
        generations.Init(2, entities.alive.Num());
        TArray<EEntityOverlayObjectiveRole> objective_roles;
        objective_roles.Init(EEntityOverlayObjectiveRole::None, entities.alive.Num());
        FRadarContactColours colours{
            .friendly = FLinearColor::Blue,
            .hostile = FLinearColor::Red,
            .neutral = FLinearColor::White,
        };
        FRadarFrame frame;
        auto& instances{frame.instances};

        static_cast<void>(collect_radar_instances(ml::test_radar_source::make_view(entities),
                                                  generations,
                                                  objective_roles,
                                                  colours,
                                                  FTransform::Identity,
                                                  {0, 2},
                                                  {},
                                                  ETestTeam::Blue,
                                                  ml::test_radar_source::linear_settings(),
                                                  frame));

        TestRunner->TestEqual(
            TEXT("Same-team contact is friendly"), instances[0].packed_color, 0xffff0000u);
        TestRunner->TestEqual(
            TEXT("Other-team contact is hostile"), instances[1].packed_color, 0xff0000ffu);
        TestRunner->TestEqual(
            TEXT("White-team contact is neutral"), instances[2].packed_color, 0xffffffffu);
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
        FRadarContactColours colours;
        FRadarFrame no_roll_frame;
        FRadarFrame rolled_frame;
        auto& no_roll_instances{no_roll_frame.instances};
        auto& rolled_instances{rolled_frame.instances};

        static_cast<void>(collect_radar_instances(ml::test_radar_source::make_view(entities),
                                                  generations,
                                                  objective_roles,
                                                  colours,
                                                  FTransform{no_roll_rotation, player_location},
                                                  {0, 3},
                                                  {},
                                                  ETestTeam::Blue,
                                                  ml::test_radar_source::linear_settings(),
                                                  no_roll_frame));
        static_cast<void>(
            collect_radar_instances(ml::test_radar_source::make_view(entities),
                                    generations,
                                    objective_roles,
                                    colours,
                                    FTransform{FRotator{28.0, 37.0, 120.0}, player_location},
                                    {0, 3},
                                    {},
                                    ETestTeam::Blue,
                                    ml::test_radar_source::linear_settings(),
                                    rolled_frame));

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

    TEST_METHOD(NonlinearMappingUsesStableSemanticAnchors)
    {
        auto const settings{ml::test_radar_source::nonlinear_settings()};
        auto const centre{ml::radar_source::to_radar_position(FVector3f::ZeroVector, settings)};
        auto const combat{
            ml::radar_source::to_radar_position({settings.combat_range, 0.0f, 0.0f}, settings)};
        auto const maximum{
            ml::radar_source::to_radar_position({settings.maximum_range, 0.0f, 0.0f}, settings)};

        TestRunner->TestTrue(TEXT("The centre maps exactly to the origin"), centre.IsZero());
        TestRunner->TestTrue(
            TEXT("The combat boundary maps to its configured radius"),
            FMath::IsNearlyEqual(combat.X, settings.combat_display_radius, UE_KINDA_SMALL_NUMBER));
        TestRunner->TestTrue(TEXT("The maximum range maps exactly to the edge"),
                             FMath::IsNearlyEqual(maximum.X, 1.0f, UE_KINDA_SMALL_NUMBER));
        TestRunner->TestTrue(TEXT("The tactical boundary maps near eighty percent"),
                             FMath::IsNearlyEqual(ml::radar_source::to_radar_display_radius(
                                                      settings.tactical_range, settings),
                                                  0.8f,
                                                  0.01f));

        auto previous_radius{0.0f};
        for (int32 distance{1}; distance <= 2000; ++distance) {
            auto const radius{
                ml::radar_source::to_radar_display_radius(static_cast<float>(distance), settings)};
            TestRunner->TestTrue(TEXT("Mapping is strictly monotonic"), radius > previous_radius);
            previous_radius = radius;
        }

        for (auto const boundary : {settings.combat_range, settings.tactical_range}) {
            auto const before{ml::radar_source::to_radar_display_radius(boundary - 1.0f, settings)};
            auto const at{ml::radar_source::to_radar_display_radius(boundary, settings)};
            auto const after{ml::radar_source::to_radar_display_radius(boundary + 1.0f, settings)};
            TestRunner->TestTrue(TEXT("Movement remains finite across a semantic boundary"),
                                 FMath::IsFinite(before) && FMath::IsFinite(at) &&
                                     FMath::IsFinite(after));
            TestRunner->TestTrue(TEXT("Movement remains smooth across a semantic boundary"),
                                 FMath::IsNearlyEqual(at - before, after - at, 0.0001f));
        }
    }

    TEST_METHOD(NonlinearMappingPreservesBearingAndCompressesHeight)
    {
        auto const settings{ml::test_radar_source::nonlinear_settings()};
        FVector2f const planar_deltas[]{
            {80.0f, 60.0f}, {-80.0f, 60.0f}, {-80.0f, -60.0f}, {80.0f, -60.0f}};
        for (auto const planar_delta : planar_deltas) {
            auto const mapped{ml::radar_source::to_radar_position(
                {planar_delta.X, planar_delta.Y, 0.0f}, settings)};
            auto const cross{planar_delta.X * mapped.Y - planar_delta.Y * mapped.X};
            auto const dot{planar_delta.X * mapped.X + planar_delta.Y * mapped.Y};
            TestRunner->TestTrue(TEXT("Planar bearing is preserved in every quadrant"),
                                 FMath::IsNearlyZero(cross, 0.001f) && dot > 0.0f);
        }

        auto previous_height{0.0f};
        for (float const height : {25.0f, 100.0f, 400.0f, 1000.0f}) {
            auto const above{ml::radar_source::to_radar_position({0.0f, 0.0f, height}, settings).Z};
            auto const below{
                ml::radar_source::to_radar_position({0.0f, 0.0f, -height}, settings).Z};
            TestRunner->TestTrue(TEXT("Vertical compression is signed and symmetric"),
                                 above > 0.0f && FMath::IsNearlyEqual(above, -below));
            TestRunner->TestTrue(TEXT("Vertical compression is strictly monotonic"),
                                 above > previous_height);
            previous_height = above;
        }
    }

    TEST_METHOD(ContactMappingDoesNotDependOnOtherContacts)
    {
        auto const settings{ml::test_radar_source::nonlinear_settings()};
        ml::entity_registry::EntityData entities;
        ml::test_radar_source::add_entity(
            entities, FVector3f::ZeroVector, ETestTeam::Blue, ETestEntityType::PlayerShip);
        ml::test_radar_source::add_entity(
            entities, {250.0f, -80.0f, 40.0f}, ETestTeam::Red, ETestEntityType::CapitalShipFighter);
        TArray<int32> generations;
        generations.Init(1, entities.alive.Num());
        TArray<EEntityOverlayObjectiveRole> objective_roles;
        objective_roles.Init(EEntityOverlayObjectiveRole::None, entities.alive.Num());
        FRadarContactColours colours;
        FRadarFrame frame;

        static_cast<void>(collect_radar_instances(ml::test_radar_source::make_view(entities),
                                                  generations,
                                                  objective_roles,
                                                  colours,
                                                  FTransform::Identity,
                                                  {0, 1},
                                                  {},
                                                  ETestTeam::Blue,
                                                  settings,
                                                  frame));
        auto const original_position{frame.instances[0].radar_position};

        ml::test_radar_source::add_entity(
            entities, {1500.0f, 100.0f, 0.0f}, ETestTeam::Green, ETestEntityType::CapitalShip);
        generations.Add(1);
        objective_roles.Add(EEntityOverlayObjectiveRole::None);
        static_cast<void>(collect_radar_instances(ml::test_radar_source::make_view(entities),
                                                  generations,
                                                  objective_roles,
                                                  colours,
                                                  FTransform::Identity,
                                                  {0, 1},
                                                  {},
                                                  ETestTeam::Blue,
                                                  settings,
                                                  frame));
        TestRunner->TestTrue(TEXT("An unrelated contact does not move an existing contact"),
                             ml::test_radar_source::nearly_equal(
                                 original_position, frame.instances[0].radar_position));
        TestRunner->TestTrue(TEXT("Frame carries the combat display radius"),
                             FMath::IsNearlyEqual(frame.combat_display_radius, 0.45f));
        TestRunner->TestTrue(TEXT("Frame carries the mapped tactical display radius"),
                             FMath::IsNearlyEqual(frame.tactical_display_radius, 0.8f, 0.01f));
    }

    TEST_METHOD(FixedMaximumRangeExcludesAllOutOfRangeContacts)
    {
        auto const settings{ml::test_radar_source::nonlinear_settings()};
        ml::entity_registry::EntityData entities;
        ml::test_radar_source::add_entity(
            entities, FVector3f::ZeroVector, ETestTeam::Blue, ETestEntityType::PlayerShip);
        ml::test_radar_source::add_entity(
            entities, {1999.0f, 0.0f, 0.0f}, ETestTeam::Red, ETestEntityType::CapitalShipFighter);
        ml::test_radar_source::add_entity(
            entities, {2000.0f, 0.0f, 0.0f}, ETestTeam::Red, ETestEntityType::CapitalShip);
        ml::test_radar_source::add_entity(
            entities, {1600.0f, 0.0f, 1200.0f}, ETestTeam::Red, ETestEntityType::Turret);
        TArray<int32> generations;
        generations.Init(1, entities.alive.Num());
        TArray<EEntityOverlayObjectiveRole> objective_roles;
        objective_roles.Init(EEntityOverlayObjectiveRole::None, entities.alive.Num());
        objective_roles[2] = EEntityOverlayObjectiveRole::Destroy;
        objective_roles[3] = EEntityOverlayObjectiveRole::Defend;
        FRadarFrame frame;

        auto const result{collect_radar_instances(ml::test_radar_source::make_view(entities),
                                                  generations,
                                                  objective_roles,
                                                  FRadarContactColours{},
                                                  FTransform::Identity,
                                                  {0, 1},
                                                  {2, 1},
                                                  ETestTeam::Blue,
                                                  settings,
                                                  frame)};
        TestRunner->TestEqual(TEXT("Only a contact inside the 3D maximum and player are visible"),
                              result.visible_count,
                              2);
        TestRunner->TestEqual(TEXT("The in-range contact remains visible"),
                              ml::test_radar_source::glyph(frame.instances[0]),
                              ERadarGlyph::Fighter);
    }
};
