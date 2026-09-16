#include <SpaceGameSimulation/entities/NativeEntityTypes.h>
#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>
#include "SpaceGamePresentation/presentation/EntityOverlaySource.h"

#include "SpaceGameSimulation/entities/TestEntityType.h"
#include "SpaceGameSimulation/entities/TestTeam.h"

#include <CQTest.h>
#include <ioj/sim/registry_entity_data.h>
#include <vector>

#include <array>
#include <ioj/sim/entity_identity_layout.h>

namespace {
using EntityTypeRadii = std::array<float, static_cast<std::size_t>(::ioj::sim::EntityType::COUNT)>;

auto make_entity_type_radii() -> EntityTypeRadii {
    EntityTypeRadii radii{};
    radii.fill(1.0f);
    return radii;
}

auto make_view(::ioj::sim::RegistryEntityData const& entities,
               std::span<::ioj::sim::EntityUniqueId const> ids = {})
    -> std::vector<::ioj::sim::AgentDisplayBatch> {
    std::vector<::ioj::sim::AgentDisplayBatch> batches;
    auto const view{entities.get_const_view()};
    auto const count{entities.num()};
    batches.reserve(count);
    for (int32 i{}; i < count; ++i) {
        auto const row{view.get_view(i, 1)};
        batches.push_back({row.entity_types[0],
                           ids.empty() ? ids : ids.subspan(i, 1),
                           row.locations,
                           row.velocities,
                           row.healths,
                           row.teams});
    }
    return batches;
}

void add_entity(::ioj::sim::RegistryEntityData& entities,
                EntityTypeRadii& entity_type_radii,
                FVector3f const position,
                int32 const health,
                ETestEntityType const type,
                float const radius = 1.0f,
                bool const alive = true,
                ETestTeam const team = ETestTeam::White) {
    auto const index{entities.num()};
    entities.add_defaulted(1);
    entities.locations.set(index, ml::to_native(position));
    entity_type_radii[static_cast<std::size_t>(type)] = radius;
    entities.healths[index] = alive ? health : 0;
    entities.teams[index] = ml::to_native(team);
    entities.entity_types[index] = ml::to_native(type);
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

auto entity_id(::ioj::sim::RegistryEntityData const& entities, int32 const index)
    -> ::ioj::sim::EntityUniqueId {
    auto const type{entities.entity_types[index]};
    return ::ioj::sim::EntityUniqueId::make(::ioj::sim::entity_identity_offset(type, index), type);
}

auto select_target(::ioj::sim::RegistryEntityData const& entities,
                   EntityTypeRadii const& entity_type_radii,
                   ::ioj::sim::EntityUniqueId const current_target = {},
                   float const weapon_range = 1000.0f) -> FSoftTargetSelectionResult {
    std::vector<::ioj::sim::EntityUniqueId> ids;
    auto const count{entities.num()};
    for (int32 i{}; i < count; ++i) {
        ids.push_back(entity_id(entities, i));
    }
    TArray<EEntityOverlayObjectiveRole> objective_roles;
    objective_roles.Init(EEntityOverlayObjectiveRole::None, entities.num());
    return select_soft_target(make_view(entities, ids),
                              entity_type_radii,
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
        ::ioj::sim::RegistryEntityData entities;
        auto entity_type_radii{make_entity_type_radii()};
        add_entity(
            entities, entity_type_radii, {10.0f, 0.0f, 0.0f}, 10, ETestEntityType::Turret, 50.0f);
        add_entity(
            entities, entity_type_radii, {20.0f, 0.0f, 0.0f}, 25, ETestEntityType::Fighter, 100.0f);
        add_entity(entities,
                   entity_type_radii,
                   {30.0f, 0.0f, 0.0f},
                   2500,
                   ETestEntityType::CapitalShip,
                   1000.0f);
        add_entity(entities,
                   entity_type_radii,
                   {40.0f, 0.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   50.0f,
                   false);
        add_entity(
            entities, entity_type_radii, {50.0f, 0.0f, 0.0f}, 100, ETestEntityType::PlayerShip);
        add_entity(
            entities, entity_type_radii, {60.0f, 0.0f, 0.0f}, 100, ETestEntityType::TubeSpinner);
        add_entity(
            entities, entity_type_radii, {101.0f, 0.0f, 0.0f}, 20, ETestEntityType::Turret, 50.0f);

        FEntityOverlayCollector collector;
        TArray<FEntityOverlayInstance> output;
        TArray<EEntityOverlayObjectiveRole> objective_roles;
        objective_roles.Init(EEntityOverlayObjectiveRole::None, entities.num());
        auto const result{collect_entity_overlay_instances(make_view(entities),
                                                           entity_type_radii,
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

    TEST_METHOD(ExcludesDeadAndClampsExcessHealth)
    {
        ::ioj::sim::RegistryEntityData entities;
        auto entity_type_radii{make_entity_type_radii()};
        add_entity(
            entities, entity_type_radii, FVector3f::ZeroVector, -10, ETestEntityType::Turret);
        add_entity(entities, entity_type_radii, FVector3f::ZeroVector, 40, ETestEntityType::Turret);

        FEntityOverlayCollector collector;
        TArray<FEntityOverlayInstance> output;
        TArray<EEntityOverlayObjectiveRole> objective_roles;
        objective_roles.Init(EEntityOverlayObjectiveRole::None, entities.num());
        static_cast<void>(collect_entity_overlay_instances(make_view(entities),
                                                           entity_type_radii,
                                                           objective_roles,
                                                           {},
                                                           {5000, 50, 20},
                                                           FVector3f::ZeroVector,
                                                           100.0f,
                                                           output,
                                                           collector));

        TestRunner->TestEqual(TEXT("Only the living entity is collected"), output.Num(), 1);
        TestRunner->TestEqual(TEXT("Excess health clamps to one"), output[0].health, 1.0f);
    }

    TEST_METHOD(SelectsOnlyTheBestValidHostile)
    {
        ::ioj::sim::RegistryEntityData entities;
        auto entity_type_radii{make_entity_type_radii()};
        add_entity(entities,
                   entity_type_radii,
                   {1000.0f, 0.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   0.0f,
                   true,
                   ETestTeam::Green);
        add_entity(entities,
                   entity_type_radii,
                   {1000.0f, 50.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   0.0f,
                   true,
                   ETestTeam::Red);
        add_entity(entities,
                   entity_type_radii,
                   {1000.0f, 80.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   0.0f,
                   true,
                   ETestTeam::Blue);
        add_entity(entities,
                   entity_type_radii,
                   {1000.0f, 0.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   0.0f,
                   false,
                   ETestTeam::Red);

        auto const selected{select_target(entities, entity_type_radii)};
        TestRunner->TestEqual(TEXT("Nearest hostile is selected"),
                              selected.id.raw_value(),
                              entity_id(entities, 1).raw_value());
    }

    TEST_METHOD(RetainsAndSwitchesWithHysteresis)
    {
        ::ioj::sim::RegistryEntityData entities;
        auto entity_type_radii{make_entity_type_radii()};
        add_entity(entities,
                   entity_type_radii,
                   {1000.0f, 65.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   0.0f,
                   true,
                   ETestTeam::Red);
        add_entity(entities,
                   entity_type_radii,
                   {1000.0f, 80.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   0.0f,
                   true,
                   ETestTeam::Blue);

        auto const retained{select_target(entities, entity_type_radii, entity_id(entities, 1))};
        TestRunner->TestEqual(TEXT("Small improvement does not switch"),
                              retained.id.raw_value(),
                              entity_id(entities, 1).raw_value());

        entities.locations.ys[0] = 50.0f;
        auto const switched{select_target(entities, entity_type_radii, entity_id(entities, 1))};
        TestRunner->TestEqual(TEXT("Materially better candidate switches"),
                              switched.id.raw_value(),
                              entity_id(entities, 0).raw_value());

        entities.locations.ys[0] = 250.0f;
        entities.locations.ys[1] = 250.0f;
        auto const cleared{select_target(entities, entity_type_radii, entity_id(entities, 1))};
        TestRunner->TestFalse(TEXT("Target outside retention is cleared"), cleared.id.is_valid());
        TestRunner->TestTrue(TEXT("Valid on-screen lost target may fade"),
                             cleared.previous_target_can_fade);

        entities.healths[1] = 0;
        auto const dead{select_target(entities, entity_type_radii, entity_id(entities, 1))};
        TestRunner->TestFalse(TEXT("Dead target may not fade"), dead.previous_target_can_fade);
    }

    TEST_METHOD(ProjectedSizeDoesNotLetARearTargetDominate)
    {
        ::ioj::sim::RegistryEntityData entities;
        auto entity_type_radii{make_entity_type_radii()};
        add_entity(entities,
                   entity_type_radii,
                   {2000.0f, 80.0f, 0.0f},
                   20,
                   ETestEntityType::CapitalShip,
                   800.0f,
                   true,
                   ETestTeam::Red);
        add_entity(entities,
                   entity_type_radii,
                   {1000.0f, 20.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   5.0f,
                   true,
                   ETestTeam::Blue);

        auto const selected{select_target(entities, entity_type_radii)};
        TestRunner->TestEqual(TEXT("Better-centred front target wins"),
                              selected.id.raw_value(),
                              entity_id(entities, 1).raw_value());
    }

    TEST_METHOD(NearestSurfaceResolvesCentredTargetsAndHysteresis)
    {
        ::ioj::sim::RegistryEntityData entities;
        auto entity_type_radii{make_entity_type_radii()};
        add_entity(entities,
                   entity_type_radii,
                   {2000.0f, 0.0f, 0.0f},
                   20,
                   ETestEntityType::CapitalShip,
                   100.0f,
                   true,
                   ETestTeam::Red);
        add_entity(entities,
                   entity_type_radii,
                   {1000.0f, 0.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   5.0f,
                   true,
                   ETestTeam::Blue);

        auto const selected{select_target(entities, entity_type_radii)};
        TestRunner->TestEqual(TEXT("Nearest tied centre wins"),
                              selected.id.raw_value(),
                              entity_id(entities, 1).raw_value());

        auto const switched{select_target(entities, entity_type_radii, entity_id(entities, 0))};
        TestRunner->TestEqual(TEXT("Materially nearer tied target replaces rear target"),
                              switched.id.raw_value(),
                              entity_id(entities, 1).raw_value());
    }

    TEST_METHOD(NormalizesRangeAlphaUsingTargetSurfaceDistance)
    {
        ::ioj::sim::RegistryEntityData entities;
        auto entity_type_radii{make_entity_type_radii()};
        add_entity(entities,
                   entity_type_radii,
                   {5000.0f, 0.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   0.0f,
                   true,
                   ETestTeam::Red);

        auto const comfortably_out_of_range{select_target(entities, entity_type_radii)};
        TestRunner->TestEqual(TEXT("Beyond transition start clamps to zero"),
                              comfortably_out_of_range.range_alpha,
                              0.0f);

        entities.locations.xs[0] = 2500.0f;
        auto const approaching{select_target(entities, entity_type_radii)};
        TestRunner->TestEqual(
            TEXT("Mid-transition range alpha is linear"), approaching.range_alpha, 0.5f);

        entities.locations.xs[0] = 1100.0f;
        entity_type_radii[static_cast<std::size_t>(ETestEntityType::Turret)] = 100.0f;
        auto const at_range_boundary{select_target(entities, entity_type_radii)};
        TestRunner->TestEqual(TEXT("Surface at range boundary has full range alpha"),
                              at_range_boundary.range_alpha,
                              1.0f);

        entities.locations.xs[0] = 500.0f;
        auto const inside_range{select_target(entities, entity_type_radii)};
        TestRunner->TestEqual(
            TEXT("Inside range remains clamped to one"), inside_range.range_alpha, 1.0f);

        auto const invalid_weapon_range{select_target(entities, entity_type_radii, {}, 0.0f)};
        TestRunner->TestEqual(TEXT("Missing weapon range produces zero range alpha"),
                              invalid_weapon_range.range_alpha,
                              0.0f);
    }

    TEST_METHOD(RangeAlphaUsesTargetSurfaceRatherThanCentre)
    {
        ::ioj::sim::RegistryEntityData entities;
        auto entity_type_radii{make_entity_type_radii()};
        add_entity(entities,
                   entity_type_radii,
                   {1100.0f, 0.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   100.0f,
                   true,
                   ETestTeam::Red);

        auto const selected{select_target(entities, entity_type_radii)};
        TestRunner->TestEqual(TEXT("Target surface at weapon range has full range alpha"),
                              selected.range_alpha,
                              1.0f);
    }

    TEST_METHOD(ReportsWorldScaleSeparatelyFromClampedIndicatorRadius)
    {
        ::ioj::sim::RegistryEntityData entities;
        auto entity_type_radii{make_entity_type_radii()};
        add_entity(entities,
                   entity_type_radii,
                   {1000.0f, 0.0f, 0.0f},
                   20,
                   ETestEntityType::Turret,
                   2.0f,
                   true,
                   ETestTeam::Red);

        auto const distant{select_target(entities, entity_type_radii)};
        TestRunner->TestTrue(TEXT("Projection scale is available for world marker scaling"),
                             distant.world_units_per_pixel > 0.0f);
        TestRunner->TestTrue(TEXT("Small projected targets still use the minimum indicator"),
                             distant.indicator_radius_pixels >
                                 2.0f / distant.world_units_per_pixel);

        entity_type_radii[static_cast<std::size_t>(ETestEntityType::Turret)] = 0.0f;
        auto const zero_radius{select_target(entities, entity_type_radii)};
        TestRunner->TestTrue(TEXT("Zero-radius targets still have a usable projection scale"),
                             zero_radius.world_units_per_pixel > 0.0f);

        entities.locations.xs[0] = 500.0f;
        auto const nearer{select_target(entities, entity_type_radii)};
        TestRunner->TestTrue(TEXT("World units per pixel shrink as the target approaches"),
                             nearer.world_units_per_pixel < distant.world_units_per_pixel);
    }
};
