#include <ioj/presentation/agent_display_batch.h>
#include <ioj/vectors3f.h>
#include <SpaceGamePresentation/presentation/EntityOverlaySource.h>

#include <CQTest.h>

#include <array>

TEST_CLASS(EntityOverlaySource, "Sandbox.UnitTests")
{
    TEST_METHOD(ConvertsCameraProjectionForNativeSelection)
    {
        ioj::Vectors3f positions;
        positions.add({1000.f, 0.f, 0.f});
        std::array const ids{ioj::sim::EntityId{0, ioj::sim::EntityType::Turret}};
        std::array const healths{ioj::sim::Health{20}};
        std::array const teams{ioj::Team::Red};
        std::array const roles{ioj::ui::entity_overlay::ObjectiveRole::None};
        std::array const batches{ioj::presentation::AgentDisplayBatch{
            ioj::sim::EntityType::Turret,
            ids,
            positions.const_view(),
            {},
            ioj::sim::HealthConstView{ioj::sim::EntityType::Turret, healths},
            teams}};
        ioj::sim::EntityTypeRadii radii{1.f};
        radii[ioj::sim::EntityType::Turret] = 100.f;

        FMatrix44f projection{ForceInitToZero};
        projection.M[1][0] = 1.f;
        projection.M[2][1] = 1.f;
        projection.M[0][2] = 1.f;
        projection.M[0][3] = 1.f;
        FSoftTargetSelectionContext const context{.view = {.camera_origin = FVector3f::ZeroVector,
                                                           .view_projection = projection,
                                                           .view_rect = FIntRect{0, 0, 1000, 1000},
                                                           .output_size = {1000, 1000}},
                                                  .player_team = ioj::ETestTeam::Green,
                                                  .effective_weapon_range = 1000.f,
                                                  .maximum_overlay_range = 10000.f};
        auto const result{select_soft_target(batches, radii, roles, context, {}, {})};
        TestRunner->TestEqual(
            TEXT("Adapter preserves entity identity"), result.id.raw_value(), ids[0].raw_value());
        TestRunner->TestEqual(
            TEXT("Adapter preserves camera projection scale"), result.world_units_per_pixel, 2.f);
        TestRunner->TestEqual(
            TEXT("Adapter preserves indicator radius"), result.indicator_radius_pixels, 60.f);
    }
};
