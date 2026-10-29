#include "ioj/sim/entities/entity_type.h"
#include "ioj/sim/entities/entity_type_radii.h"
#include "SpaceGamePresentation/entities/TestTeam.h"
#include "SpaceGamePresentation/presentation/EntityOverlaySource.h"
#include <ioj/column_math.h>
#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/entities/entity_identity_layout.h>
#include <ioj/sim/lasers/laser_source.h>
#include <SandboxTests/support/DisplayEntityTestData.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>

#include <SandboxCoreEngine/VectorConversion.h>

#include <CQTest.h>

#include <vector>

namespace {
using EntityTypeRadii = ::ioj::sim::EntityTypeRadii;

auto make_entity_type_radii() -> EntityTypeRadii {
    EntityTypeRadii radii{};
    for (auto const type : ioj::EnumTraits<::ioj::sim::EntityType>::values) {
        radii[type] = 1.0f;
    }
    return radii;
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

struct FOverlayScene {
    void add_entity(FVector3f const position,
                    int32 const health,
                    ioj::sim::EntityType const type,
                    float const radius = 1.0f,
                    bool const alive = true,
                    ETestTeam const team = ETestTeam::White) {
        auto const index{entities.num()};
        entities.add_defaulted(1);
        ioj::set_vector(
            entities.motion.get_view().view_locations(), index, ioj::Vector3f{position});
        radii[type] = radius;
        entities.teams[index] = ioj::to_native(team);
        entities.entity_types[index] = type;
        entities.add_health(
            index, ioj::sim::EntityId(static_cast<std::uint32_t>(index), type), alive ? health : 0);
    }

    auto collect(TArray<FEntityOverlayInstance>& output, FEntityOverlayCollector& collector) const {
        TArray<EEntityOverlayObjectiveRole> objective_roles;
        objective_roles.Init(EEntityOverlayObjectiveRole::None, entities.num());
        return collect_entity_overlay_instances(entities.make_batches(),
                                                radii,
                                                objective_roles,
                                                {},
                                                {5000, 50, 20},
                                                FVector3f::ZeroVector,
                                                100.0f,
                                                output,
                                                collector);
    }

    ioj::tests::FDisplayEntityTestData entities;
    EntityTypeRadii radii{make_entity_type_radii()};
};

auto select_target(ioj::tests::FDisplayEntityTestData const& entities,
                   EntityTypeRadii const& entity_type_radii,
                   ::ioj::sim::EntityId const current_target = {},
                   float const weapon_range = 1000.0f) -> ioj::presentation::SoftTargetResult {
    TArray<EEntityOverlayObjectiveRole> objective_roles;
    objective_roles.Init(EEntityOverlayObjectiveRole::None, entities.num());
    return select_soft_target(entities.make_batches(entities.entity_ids),
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

TEST_CLASS(EntityOverlaySource, "Sandbox.UnitTests")
{
    TEST_METHOD(FiltersEligibilityRangeAndNormalizesHealth)
    {
        FOverlayScene scene;
        scene.add_entity({10.0f, 0.0f, 0.0f}, 10, ::ioj::sim::EntityType::Turret, 50.0f);
        scene.add_entity({20.0f, 0.0f, 0.0f}, 25, ::ioj::sim::EntityType::Fighter, 100.0f);
        scene.add_entity({30.0f, 0.0f, 0.0f}, 2500, ::ioj::sim::EntityType::CapitalShip, 1000.0f);
        scene.add_entity({40.0f, 0.0f, 0.0f}, 20, ::ioj::sim::EntityType::Turret, 50.0f, false);
        scene.add_entity({50.0f, 0.0f, 0.0f}, 100, ::ioj::sim::EntityType::PlayerShip);
        scene.add_entity({60.0f, 0.0f, 0.0f}, 100, ::ioj::sim::EntityType::TubeSpinner);
        scene.add_entity({101.0f, 0.0f, 0.0f}, 20, ::ioj::sim::EntityType::Turret, 50.0f);

        FEntityOverlayCollector collector;
        TArray<FEntityOverlayInstance> output;
        auto const result{scene.collect(output, collector)};

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
        FOverlayScene scene;
        scene.add_entity(FVector3f::ZeroVector, -10, ::ioj::sim::EntityType::Turret);
        scene.add_entity(FVector3f::ZeroVector, 40, ::ioj::sim::EntityType::Turret);

        FEntityOverlayCollector collector;
        TArray<FEntityOverlayInstance> output;
        static_cast<void>(scene.collect(output, collector));

        TestRunner->TestEqual(TEXT("Only the living entity is collected"), output.Num(), 1);
        TestRunner->TestEqual(TEXT("Excess health clamps to one"), output[0].health, 1.0f);
    }

    TEST_METHOD(ConvertsCameraProjectionForNativeSelection)
    {
        FOverlayScene scene;
        scene.add_entity(
            {1000.f, 0.f, 0.f}, 20, ioj::sim::EntityType::Turret, 100.f, true, ETestTeam::Red);
        auto const result{select_target(scene.entities, scene.radii)};
        TestRunner->TestEqual(TEXT("Adapter preserves entity identity"),
                              result.id.raw_value(),
                              scene.entities.entity_ids[0].raw_value());
        TestRunner->TestEqual(
            TEXT("Adapter preserves camera projection scale"), result.world_units_per_pixel, 2.f);
        TestRunner->TestEqual(
            TEXT("Adapter preserves indicator radius"), result.indicator_radius_pixels, 60.f);
    }
};
