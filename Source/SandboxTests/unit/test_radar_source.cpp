#include <ioj/column_math.h>
#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/entities/entity_identity_layout.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/sim/lasers/laser_source.h>
#include <SandboxTests/support/DisplayEntityTestData.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>
#include <SpaceGamePresentation/presentation/RadarSource.h>

#include <SandboxCoreEngine/VectorConversion.h>

#include <CQTest.h>

#include <vector>

namespace ioj::test_radar_source {
auto glyph(FRadarInstance const& instance) -> ERadarGlyph {
    return static_cast<ERadarGlyph>(instance.packed_glyph_and_flags & 0xffu);
}

auto flags(FRadarInstance const& instance) -> ERadarContactFlags {
    return static_cast<ERadarContactFlags>(instance.packed_glyph_and_flags >> 8u);
}

auto nearly_equal(FVector3f const lhs, FVector3f const rhs) -> bool {
    return lhs.Equals(rhs, 0.0001f);
}

auto linear_settings(float const maximum_range = 100.0f) -> ioj::settings::Radar {
    ioj::settings::Radar settings;
    settings.combat_range = maximum_range * 0.25f;
    settings.tactical_range = maximum_range * 0.5f;
    settings.maximum_range = maximum_range;
    settings.combat_display_radius = 0.25f;
    return ioj::presentation::sanitize_radar_settings(settings);
}

auto nonlinear_settings() -> ioj::settings::Radar {
    ioj::settings::Radar settings;
    settings.combat_range = 100.0f;
    settings.tactical_range = 400.0f;
    settings.maximum_range = 2000.0f;
    settings.combat_display_radius = 0.45f;
    return ioj::presentation::sanitize_radar_settings(settings);
}
struct FRadarScene {
    void add_entity(FVector3f const location,
                    ETestTeam const team,
                    sim::EntityType const type,
                    bool const alive = true) {
        auto const index{entities.num()};
        entities.add_defaulted(1);
        set_vector(entities.motion.get_view().view_locations(), index, Vector3f{location});
        entities.teams[index] = to_native(team);
        entities.entity_types[index] = type;
        entities.add_health(
            index, sim::EntityId(static_cast<std::uint32_t>(index), type), alive ? 100 : 0);
        objective_roles.Add(EEntityOverlayObjectiveRole::None);
    }

    auto collect(FRadarFrame& frame,
                 settings::Radar const& settings = linear_settings(),
                 FTransform const& transform = FTransform::Identity) const {
        return collect_radar_instances(entities.make_batches(entities.entity_ids),
                                       objective_roles,
                                       colours,
                                       transform,
                                       entities.entity_ids[0],
                                       ETestTeam::Blue,
                                       settings,
                                       frame);
    }

    tests::FDisplayEntityTestData entities;
    TArray<EEntityOverlayObjectiveRole> objective_roles;
    FRadarContactColours colours;
};
} // namespace ioj::test_radar_source

TEST_CLASS(RadarSource, "Sandbox.UnitTests")
{
    TEST_METHOD(FiltersMapsAndOrdersContacts)
    {
        ioj::test_radar_source::FRadarScene scene;
        scene.add_entity(
            FVector3f::ZeroVector, ETestTeam::Blue, ::ioj::sim::EntityType::PlayerShip);
        scene.add_entity({50.0f, 0.0f, 0.0f}, ETestTeam::Red, ::ioj::sim::EntityType::Fighter);
        scene.add_entity(
            {0.0f, 50.0f, 25.0f}, ETestTeam::Green, ::ioj::sim::EntityType::CapitalShip);
        scene.add_entity({-50.0f, 0.0f, 0.0f}, ETestTeam::Yellow, ::ioj::sim::EntityType::Turret);
        scene.add_entity(
            {0.0f, -50.0f, 0.0f}, ETestTeam::White, ::ioj::sim::EntityType::TubeSpinner);
        scene.add_entity({100.0f, 0.0f, 0.0f}, ETestTeam::Red, ::ioj::sim::EntityType::Turret);
        scene.add_entity(
            {25.0f, 0.0f, 0.0f}, ETestTeam::Red, ::ioj::sim::EntityType::Fighter, false);
        scene.add_entity({101.0f, 0.0f, 0.0f}, ETestTeam::Red, ::ioj::sim::EntityType::CapitalShip);

        scene.objective_roles[2] = EEntityOverlayObjectiveRole::Defend;
        scene.objective_roles[3] = EEntityOverlayObjectiveRole::Destroy;
        scene.objective_roles[7] = EEntityOverlayObjectiveRole::Defend;

        scene.colours.hostile = FLinearColor::Red;

        FRadarFrame frame;
        auto& instances{frame.instances};
        auto const result{scene.collect(frame)};

        TestRunner->TestEqual(
            TEXT("All alive non-player rows are candidates"), result.candidate_count, 6);
        TestRunner->TestEqual(
            TEXT("Only in-range contacts and the player are visible"), result.visible_count, 5);
        TestRunner->TestEqual(TEXT("Normal fighter is drawn first"),
                              ioj::test_radar_source::glyph(instances[0]),
                              ERadarGlyph::Fighter);
        TestRunner->TestEqual(TEXT("Unsupported type maps to unknown"),
                              ioj::test_radar_source::glyph(instances[1]),
                              ERadarGlyph::Unknown);
        TestRunner->TestEqual(TEXT("Objective follows normal contacts"),
                              ioj::test_radar_source::glyph(instances[2]),
                              ERadarGlyph::CapitalShip);
        TestRunner->TestEqual(TEXT("Objective preserves its role"),
                              ioj::test_radar_source::flags(instances[2]),
                              ERadarContactFlags::DefendObjective);
        TestRunner->TestEqual(TEXT("Second objective preserves input order"),
                              ioj::test_radar_source::glyph(instances[3]),
                              ERadarGlyph::Turret);
        TestRunner->TestEqual(TEXT("Second objective preserves its role"),
                              ioj::test_radar_source::flags(instances[3]),
                              ERadarContactFlags::DestroyObjective);
        TestRunner->TestEqual(TEXT("Player is always last"),
                              ioj::test_radar_source::glyph(instances.Last()),
                              ERadarGlyph::Player);
        TestRunner->TestTrue(TEXT("Player is central"),
                             instances.Last().radar_position.IsNearlyZero());
        TestRunner->TestEqual(TEXT("Hostile fighter uses the hostile colour"),
                              instances[0].packed_color,
                              0xff0000ffu);
        TestRunner->TestEqual(TEXT("Blue occupies the shader's blue channel"),
                              pack_radar_color(FLinearColor{0.0f, 0.0f, 1.0f, 1.0f}),
                              0xffff0000u);

        auto zero_range_settings{ioj::test_radar_source::linear_settings()};
        zero_range_settings.maximum_range = UE_SMALL_NUMBER;
        zero_range_settings.combat_range = UE_SMALL_NUMBER;
        zero_range_settings.tactical_range = UE_SMALL_NUMBER;
        zero_range_settings.combat_display_radius = 1.0f;
        auto const zero_range_result{scene.collect(frame, zero_range_settings)};
        TestRunner->TestEqual(
            TEXT("Zero range retains only the player"), zero_range_result.visible_count, 1);
    }

    TEST_METHOD(UsesPlayerRelativeContactColours)
    {
        ioj::test_radar_source::FRadarScene scene;
        scene.add_entity(
            FVector3f::ZeroVector, ETestTeam::Blue, ::ioj::sim::EntityType::PlayerShip);
        scene.add_entity({10.0f, 0.0f, 0.0f}, ETestTeam::Blue, ::ioj::sim::EntityType::Fighter);
        scene.add_entity({20.0f, 0.0f, 0.0f}, ETestTeam::Red, ::ioj::sim::EntityType::CapitalShip);
        scene.add_entity({30.0f, 0.0f, 0.0f}, ETestTeam::White, ::ioj::sim::EntityType::Turret);

        scene.colours = FRadarContactColours{
            .friendly = FLinearColor::Blue,
            .hostile = FLinearColor::Red,
            .neutral = FLinearColor::White,
        };
        FRadarFrame frame;
        auto& instances{frame.instances};

        static_cast<void>(scene.collect(frame));

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

        ioj::test_radar_source::FRadarScene scene;
        scene.add_entity(
            FVector3f{player_location}, ETestTeam::Blue, ::ioj::sim::EntityType::PlayerShip);
        scene.add_entity(world_contact, ETestTeam::Red, ::ioj::sim::EntityType::Fighter);
        FRadarFrame no_roll_frame;
        FRadarFrame rolled_frame;
        auto& no_roll_instances{no_roll_frame.instances};
        auto& rolled_instances{rolled_frame.instances};

        static_cast<void>(scene.collect(no_roll_frame,
                                        ioj::test_radar_source::linear_settings(),
                                        FTransform{no_roll_rotation, player_location}));
        static_cast<void>(scene.collect(rolled_frame,
                                        ioj::test_radar_source::linear_settings(),
                                        FTransform{FRotator{28.0, 37.0, 120.0}, player_location}));

        TestRunner->TestEqual(
            TEXT("Each orientation emits contact and player"), no_roll_instances.Num(), 2);
        TestRunner->TestEqual(
            TEXT("Rolled orientation emits contact and player"), rolled_instances.Num(), 2);
        TestRunner->TestTrue(
            TEXT("Roll does not rotate the contact field"),
            ioj::test_radar_source::nearly_equal(no_roll_instances[0].radar_position,
                                                 rolled_instances[0].radar_position));
        TestRunner->TestTrue(
            TEXT("Yaw and pitch recover player-relative height"),
            FMath::IsNearlyEqual(no_roll_instances[0].radar_position.Z, 0.2f, 0.0001f));
        TestRunner->TestTrue(TEXT("Forward remains positive on the radar"),
                             no_roll_instances[0].radar_position.X > 0.0f);
        TestRunner->TestTrue(TEXT("Right remains positive on the radar"),
                             no_roll_instances[0].radar_position.Y > 0.0f);
    }

    TEST_METHOD(ContactMappingDoesNotDependOnOtherContacts)
    {
        auto const settings{ioj::test_radar_source::nonlinear_settings()};
        ioj::test_radar_source::FRadarScene scene;
        scene.add_entity(
            FVector3f::ZeroVector, ETestTeam::Blue, ::ioj::sim::EntityType::PlayerShip);
        scene.add_entity({250.0f, -80.0f, 40.0f}, ETestTeam::Red, ::ioj::sim::EntityType::Fighter);
        FRadarFrame frame;

        static_cast<void>(scene.collect(frame, settings));
        auto const original_position{frame.instances[0].radar_position};

        scene.add_entity(
            {1500.0f, 100.0f, 0.0f}, ETestTeam::Green, ::ioj::sim::EntityType::CapitalShip);
        static_cast<void>(scene.collect(frame, settings));
        TestRunner->TestTrue(TEXT("An unrelated contact does not move an existing contact"),
                             ioj::test_radar_source::nearly_equal(
                                 original_position, frame.instances[0].radar_position));
        TestRunner->TestTrue(TEXT("Frame carries the combat display radius"),
                             FMath::IsNearlyEqual(frame.combat_display_radius, 0.45f));
        TestRunner->TestTrue(TEXT("Frame carries the mapped tactical display radius"),
                             FMath::IsNearlyEqual(frame.tactical_display_radius, 0.8f, 0.01f));
    }

    TEST_METHOD(FixedMaximumRangeExcludesAllOutOfRangeContacts)
    {
        auto const settings{ioj::test_radar_source::nonlinear_settings()};
        ioj::test_radar_source::FRadarScene scene;
        scene.add_entity(
            FVector3f::ZeroVector, ETestTeam::Blue, ::ioj::sim::EntityType::PlayerShip);
        scene.add_entity({1999.0f, 0.0f, 0.0f}, ETestTeam::Red, ::ioj::sim::EntityType::Fighter);
        scene.add_entity(
            {2000.0f, 0.0f, 0.0f}, ETestTeam::Red, ::ioj::sim::EntityType::CapitalShip);
        scene.add_entity({1600.0f, 0.0f, 1200.0f}, ETestTeam::Red, ::ioj::sim::EntityType::Turret);
        scene.objective_roles[2] = EEntityOverlayObjectiveRole::Destroy;
        scene.objective_roles[3] = EEntityOverlayObjectiveRole::Defend;
        FRadarFrame frame;

        auto const result{scene.collect(frame, settings)};
        TestRunner->TestEqual(TEXT("Only a contact inside the 3D maximum and player are visible"),
                              result.visible_count,
                              2);
        TestRunner->TestEqual(TEXT("The in-range contact remains visible"),
                              ioj::test_radar_source::glyph(frame.instances[0]),
                              ERadarGlyph::Fighter);
    }
};
