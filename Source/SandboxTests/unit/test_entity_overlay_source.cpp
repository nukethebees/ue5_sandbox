#include "SpaceGame/presentation/EntityOverlaySource.h"

#include "SpaceGame/entities/TestEntityType.h"
#include "SpaceGame/entities/TestTeam.h"

#include <CQTest.h>

namespace {
auto make_view(ml::entity_registry::EntityData const& entities)
    -> ml::entity_registry::EntityData::ConstView {
    return {.locations = {entities.locations.xs, entities.locations.ys, entities.locations.zs},
            .velocities = {entities.velocities.xs, entities.velocities.ys, entities.velocities.zs},
            .rotations = entities.rotations.get_const_view(),
            .radii = entities.radii,
            .healths = entities.healths,
            .teams = entities.teams,
            .entity_types = entities.entity_types,
            .alive = entities.alive};
}

void add_entity(ml::entity_registry::EntityData& entities,
                FVector3f const position,
                int32 const health,
                ETestEntityType const type,
                float const radius = 1.0f,
                bool const alive = true,
                ETestTeam const team = ETestTeam::White) {
    entities.locations.add(position);
    entities.velocities.add(FVector3f::ZeroVector);
    entities.rotations.add_zeroed(1);
    entities.radii.Add(radius);
    entities.healths.Add(health);
    entities.teams.Add(team);
    entities.entity_types.Add(type);
    entities.alive.Add(alive ? 1 : 0);
}

auto make_forward_x_view() -> FEntityOverlayView {
    FMatrix44f projection{FMatrix44f::Identity};
    FMemory::Memzero(projection.M, sizeof(projection.M));
    projection.M[1][0] = 1.0f;
    projection.M[2][1] = 1.0f;
    projection.M[0][2] = 1.0f;
    projection.M[0][3] = 1.0f;
    return {.camera_origin = FVector3f::ZeroVector,
            .view_projection = projection,
            .view_rect = FIntRect{0, 0, 1000, 1000},
            .output_size = {1000, 1000}};
}

auto select_target(ml::entity_registry::EntityData const& entities,
                   FRegistryEntityHandle const current_target = {},
                   float const weapon_range = 1000.0f) -> FSoftTargetSelectionResult {
    TArray<int> generations;
    generations.Init(0, entities.alive.Num());
    TArray<EEntityOverlayObjectiveRole> objective_roles;
    objective_roles.Init(EEntityOverlayObjectiveRole::None, entities.alive.Num());
    return select_soft_target(make_view(entities),
                              generations,
                              objective_roles,
                              {.view = make_forward_x_view(),
                               .aim_origin = FVector3f::ZeroVector,
                               .aim_direction = {1.0f, 0.0f, 0.0f},
                               .camera_right = {0.0f, 1.0f, 0.0f},
                               .camera_up = {0.0f, 0.0f, 1.0f},
                               .player_team = ETestTeam::Green,
                               .effective_weapon_range = weapon_range,
                               .maximum_overlay_range = 10000.0f},
                              {},
                              current_target);
}
}

TEST_CLASS(EntityOverlayRegistrySource, "Sandbox.UnitTests")
{
    TEST_METHOD(FiltersEligibilityRangeAndNormalizesHealth)
    {
        ml::entity_registry::EntityData entities;
        add_entity(entities, {10.0f, 0.0f, 0.0f}, 10, ETestEntityType::Turret, 50.0f);
        add_entity(entities, {20.0f, 0.0f, 0.0f}, 25, ETestEntityType::CapitalShipFighter, 100.0f);
        add_entity(entities, {30.0f, 0.0f, 0.0f}, 2500, ETestEntityType::CapitalShip, 1000.0f);
        add_entity(entities, {40.0f, 0.0f, 0.0f}, 20, ETestEntityType::Turret, 50.0f, false);
        add_entity(entities, {50.0f, 0.0f, 0.0f}, 100, ETestEntityType::PlayerShip);
        add_entity(entities, {60.0f, 0.0f, 0.0f}, 100, ETestEntityType::TubeSpinner);
        add_entity(entities, {101.0f, 0.0f, 0.0f}, 20, ETestEntityType::Turret);

        FEntityOverlayCollector collector;
        TArray<FEntityOverlayInstance> output;
        TArray<EEntityOverlayObjectiveRole> objective_roles;
        objective_roles.Init(EEntityOverlayObjectiveRole::None, entities.alive.Num());
        auto const result{collect_entity_overlay_instances(make_view(entities),
                                                           objective_roles,
                                                           {},
                                                           {5000, 50, 20},
                                                           FVector3f::ZeroVector,
                                                           100.0f,
                                                           output,
                                                           collector)};

        TestRunner->TestEqual(TEXT("Only alive supported in-range entities are collected"),
                              result.candidate_count,
                              3);
        TestRunner->TestEqual(TEXT("Turret health is normalized"), output[0].health, 0.5f);
        TestRunner->TestEqual(TEXT("Fighter health is normalized"), output[1].health, 0.5f);
        TestRunner->TestEqual(TEXT("Capital health is normalized"), output[2].health, 0.5f);
        TestRunner->TestEqual(TEXT("Turret radius is retained"), output[0].world_radius, 50.0f);
        TestRunner->TestEqual(TEXT("Fighter radius is retained"), output[1].world_radius, 100.0f);
        TestRunner->TestEqual(TEXT("Capital radius is retained"), output[2].world_radius, 1000.0f);
    }

    TEST_METHOD(ClampsHealthFromRegistry)
    {
        ml::entity_registry::EntityData entities;
        add_entity(entities, FVector3f::ZeroVector, -10, ETestEntityType::Turret);
        add_entity(entities, FVector3f::ZeroVector, 40, ETestEntityType::Turret);

        FEntityOverlayCollector collector;
        TArray<FEntityOverlayInstance> output;
        TArray<EEntityOverlayObjectiveRole> objective_roles;
        objective_roles.Init(EEntityOverlayObjectiveRole::None, entities.alive.Num());
        static_cast<void>(collect_entity_overlay_instances(make_view(entities),
                                                           objective_roles,
                                                           {},
                                                           {5000, 50, 20},
                                                           FVector3f::ZeroVector,
                                                           100.0f,
                                                           output,
                                                           collector));

        TestRunner->TestEqual(TEXT("Negative health clamps to zero"), output[0].health, 0.0f);
        TestRunner->TestEqual(TEXT("Excess health clamps to one"), output[1].health, 1.0f);
    }

    TEST_METHOD(SelectsOnlyTheBestValidHostile)
    {
        ml::entity_registry::EntityData entities;
        add_entity(entities,
                   {1000.0f, 0.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   0.0f,
                   true,
                   ETestTeam::Green);
        add_entity(entities,
                   {1000.0f, 50.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   0.0f,
                   true,
                   ETestTeam::Red);
        add_entity(entities,
                   {1000.0f, 80.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   0.0f,
                   true,
                   ETestTeam::Blue);
        add_entity(entities,
                   {1000.0f, 0.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   0.0f,
                   false,
                   ETestTeam::Red);

        auto const selected{select_target(entities)};
        TestRunner->TestEqual(TEXT("Nearest hostile is selected"), selected.handle.index, 1);
    }

    TEST_METHOD(RetainsAndSwitchesWithHysteresis)
    {
        ml::entity_registry::EntityData entities;
        add_entity(entities,
                   {1000.0f, 65.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   0.0f,
                   true,
                   ETestTeam::Red);
        add_entity(entities,
                   {1000.0f, 80.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   0.0f,
                   true,
                   ETestTeam::Blue);

        auto const retained{select_target(entities, {1, 0})};
        TestRunner->TestEqual(TEXT("Small improvement does not switch"), retained.handle.index, 1);

        entities.locations.ys[0] = 50.0f;
        auto const switched{select_target(entities, {1, 0})};
        TestRunner->TestEqual(
            TEXT("Materially better candidate switches"), switched.handle.index, 0);

        entities.locations.ys[0] = 250.0f;
        entities.locations.ys[1] = 250.0f;
        auto const cleared{select_target(entities, {1, 0})};
        TestRunner->TestFalse(TEXT("Target outside retention is cleared"),
                              cleared.handle.is_valid());
        TestRunner->TestTrue(TEXT("Valid on-screen lost target may fade"),
                             cleared.previous_target_can_fade);

        entities.alive[1] = 0;
        auto const dead{select_target(entities, {1, 0})};
        TestRunner->TestFalse(TEXT("Dead target may not fade"), dead.previous_target_can_fade);
    }

    TEST_METHOD(ProjectedSizeDoesNotLetARearTargetDominate)
    {
        ml::entity_registry::EntityData entities;
        add_entity(entities,
                   {2000.0f, 80.0f, 0.0f},
                   20,
                   ETestEntityType::CapitalShip,
                   800.0f,
                   true,
                   ETestTeam::Red);
        add_entity(entities,
                   {1000.0f, 20.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   5.0f,
                   true,
                   ETestTeam::Blue);

        auto const selected{select_target(entities)};
        TestRunner->TestEqual(TEXT("Better-centred front target wins"), selected.handle.index, 1);
    }

    TEST_METHOD(NearestSurfaceResolvesCentredTargetsAndHysteresis)
    {
        ml::entity_registry::EntityData entities;
        add_entity(entities,
                   {2000.0f, 0.0f, 0.0f},
                   20,
                   ETestEntityType::CapitalShip,
                   100.0f,
                   true,
                   ETestTeam::Red);
        add_entity(entities,
                   {1000.0f, 0.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   5.0f,
                   true,
                   ETestTeam::Blue);

        auto const selected{select_target(entities)};
        TestRunner->TestEqual(TEXT("Nearest tied centre wins"), selected.handle.index, 1);

        auto const switched{select_target(entities, {0, 0})};
        TestRunner->TestEqual(
            TEXT("Materially nearer tied target replaces rear target"), switched.handle.index, 1);
    }

    TEST_METHOD(ShapesProgressAndUsesTargetSurfaceForRange)
    {
        ml::entity_registry::EntityData entities;
        add_entity(entities,
                   {2500.0f, 0.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   0.0f,
                   true,
                   ETestTeam::Red);

        auto approaching{select_target(entities)};
        TestRunner->TestEqual(
            TEXT("Mid-approach progress is linear"), approaching.range_progress, 0.5f);
        TestRunner->TestFalse(TEXT("Mid-approach target is out of range"), approaching.in_range);

        entities.locations.xs[0] = 1100.0f;
        entities.radii[0] = 100.0f;
        auto const in_range{select_target(entities)};
        TestRunner->TestTrue(TEXT("Surface at range boundary is engageable"), in_range.in_range);
        TestRunner->TestEqual(TEXT("In-range progress is complete"), in_range.range_progress, 1.0f);
    }

    TEST_METHOD(MarksActiveAndFadingOverlayInstances)
    {
        ml::entity_registry::EntityData entities;
        add_entity(entities, {10.0f, 0.0f, 0.0f}, 20, ETestEntityType::Turret);
        add_entity(entities, {20.0f, 0.0f, 0.0f}, 20, ETestEntityType::Turret);
        add_entity(entities, {30.0f, 0.0f, 0.0f}, 20, ETestEntityType::Turret);

        FEntityOverlayCollector collector;
        TArray<FEntityOverlayInstance> output;
        TArray<EEntityOverlayObjectiveRole> objective_roles;
        objective_roles.Init(EEntityOverlayObjectiveRole::None, entities.alive.Num());
        static_cast<void>(collect_entity_overlay_instances(make_view(entities),
                                                           objective_roles,
                                                           {},
                                                           {5000, 50, 20},
                                                           FVector3f::ZeroVector,
                                                           100.0f,
                                                           output,
                                                           collector,
                                                           1,
                                                           2));

        TestRunner->TestFalse(TEXT("Unselected instance is unmarked"), output[0].is_soft_target());
        TestRunner->TestTrue(TEXT("Selected instance is marked"), output[1].is_soft_target());
        TestRunner->TestEqual(TEXT("Selected instance is active"),
                              output[1].soft_target_role(),
                              EEntityOverlaySoftTargetRole::Active);
        TestRunner->TestEqual(TEXT("Previous instance is fading"),
                              output[2].soft_target_role(),
                              EEntityOverlaySoftTargetRole::Fading);
    }
};
