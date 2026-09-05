#include "SpaceGame/presentation/EntityOverlaySource.h"

#include "SpaceGame/entities/TestEntityType.h"
#include "SpaceGame/entities/TestTeam.h"

#include <CQTest.h>

namespace {
auto make_view(ml::entity_registry::EntityData const& entities)
    -> ml::entity_registry::EntityData::ConstView {
    return {.locations = {entities.locations.xs, entities.locations.ys, entities.locations.zs},
            .velocities = {entities.velocities.xs, entities.velocities.ys, entities.velocities.zs},
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
                bool const alive = true) {
    entities.locations.add(position);
    entities.velocities.add(FVector3f::ZeroVector);
    entities.radii.Add(radius);
    entities.healths.Add(health);
    entities.teams.Add(ETestTeam::White);
    entities.entity_types.Add(type);
    entities.alive.Add(alive ? 1 : 0);
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
        auto const result{collect_entity_overlay_instances(
            make_view(entities), {5000, 50, 20}, FVector3f::ZeroVector, 100.0f, output, collector)};

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
        static_cast<void>(collect_entity_overlay_instances(
            make_view(entities), {5000, 50, 20}, FVector3f::ZeroVector, 100.0f, output, collector));

        TestRunner->TestEqual(TEXT("Negative health clamps to zero"), output[0].health, 0.0f);
        TestRunner->TestEqual(TEXT("Excess health clamps to one"), output[1].health, 1.0f);
    }
};
