#include "SandboxEditor/Commandlets/EntityOverlayBenchmarkCommandlet.h"

#include "ioj/sim/entities/entity_identity_layout.h"
#include "ioj/sim/entities/entity_type_radii.h"
#include "ioj/sim/turrets/turret_entity_data.h"
#include <ioj/column_math.h>
#include <ioj/presentation/entity_overlay.h>
#include <ioj/presentation/entity_overlay_health_maximums.h>
#include <ioj/presentation/entity_overlay_team_colours.h>

#include "SandboxUI/EntityOverlay/EntityOverlayBenchmark.h"
#include <SandboxCoreEngine/VectorConversion.h>

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RHIGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogEntityOverlayBenchmark, Log, All);

namespace {
auto make_view(::ioj::sim::TurretEntityData const& entities,
               ::ioj::sim::HealthTable const& health_table)
    -> std::array<::ioj::presentation::AgentDisplayBatch, 1> {
    auto const data{entities.get_const_view()};
    return {{{::ioj::sim::EntityType::Turret,
              data.entity_ids(),
              data.view_locations(),
              {},
              health_table.get_const_view<::ioj::sim::EntityType::Turret>(data.num()),
              data.teams()}}};
}

auto make_team_colours() -> ioj::presentation::EntityOverlayTeamColours {
    ioj::EnumArray<ioj::Team, ioj::ui::Color4f> colours;
    colours[ioj::Team::White] = {1.f, 1.f, 1.f, 1.f};
    colours[ioj::Team::Red] = {1.f, 0.f, 0.f, 1.f};
    colours[ioj::Team::Green] = {0.f, 1.f, 0.f, 1.f};
    colours[ioj::Team::Blue] = {0.f, 0.f, 1.f, 1.f};
    colours[ioj::Team::Orange] = {1.f, 0.35f, 0.f, 1.f};
    colours[ioj::Team::Yellow] = {1.f, 1.f, 0.f, 1.f};
    return {.capital_ship = colours, .fighter = colours, .turret = colours};
}

auto write_debug_frames(FString const& output_directory) -> bool {
    ::ioj::sim::TurretEntityData entities;
    ::ioj::sim::HealthTable health_table;
    FVector3f const positions[]{
        {1000.0f, -600.0f, 0.0f},
        {1000.0f, -300.0f, 0.0f},
        {1000.0f, 0.0f, 0.0f},
        {1000.0f, 300.0f, 0.0f},
        {1000.0f, 600.0f, 0.0f},
        {1000.0f, 950.0f, 200.0f},
        {-100.0f, 0.0f, 300.0f},
        {1000.0f, 1500.0f, -200.0f},
        {12000.0f, 0.0f, 0.0f},
        {-10.0f, -5.0f, -5.0f},
    };
    ::ioj::sim::Health const health[]{::ioj::sim::Health{0},
                                      ::ioj::sim::Health{25},
                                      ::ioj::sim::Health{50},
                                      ::ioj::sim::Health{75},
                                      ::ioj::sim::Health{100},
                                      ::ioj::sim::Health{75},
                                      ::ioj::sim::Health{100},
                                      ::ioj::sim::Health{100},
                                      ::ioj::sim::Health{100},
                                      ::ioj::sim::Health{50}};
    ::ioj::sim::EntityTypeRadii entity_type_radii;
    for (auto const type : ioj::EnumTraits<::ioj::sim::EntityType>::values) {
        entity_type_radii[type] = 200.0f;
    }
    auto const count{UE_ARRAY_COUNT(positions)};
    entities.add_defaulted(count);
    for (int32 index{0}; index < count; ++index) {
        entities.get_view().entity_ids()[index] =
            ::ioj::sim::EntityId(static_cast<std::uint32_t>(index), ::ioj::sim::EntityType::Turret);
        ::ioj::set_vector(
            entities.get_view().view_locations(), index, ioj::Vector3f{positions[index]});
        entities.get_view().teams()[index] =
            static_cast<::ioj::Team>(index % ioj::enum_count<::ioj::Team>());
    }
    health_table.initialise_rows<::ioj::sim::EntityType::Turret>(0, health);

    FMatrix44f projection{FMatrix44f::Identity};
    FMemory::Memzero(projection.M, sizeof(projection.M));
    projection.M[1][0] = 1.0f;
    projection.M[2][1] = 1.0f;
    projection.M[0][2] = 1.0f;
    projection.M[0][3] = 1.0f;
    FEntityOverlayView const view{.camera_origin = FVector3f::ZeroVector,
                                  .view_projection = projection,
                                  .view_rect = FIntRect{0, 0, 1280, 720},
                                  .output_size = {1280, 720}};

    std::vector<ioj::ui::entity_overlay::Instance> collected;
    auto frame_store{MakeShared<FEntityOverlayFrameStore, ESPMode::ThreadSafe>()};
    TArray<ioj::ui::entity_overlay::ObjectiveRole> objective_roles;
    objective_roles.Init(ioj::ui::entity_overlay::ObjectiveRole::None, count);
    objective_roles[1] = ioj::ui::entity_overlay::ObjectiveRole::Defend;
    objective_roles[2] = ioj::ui::entity_overlay::ObjectiveRole::Destroy;
    objective_roles[5] = ioj::ui::entity_overlay::ObjectiveRole::Defend;
    objective_roles[6] = ioj::ui::entity_overlay::ObjectiveRole::Destroy;
    objective_roles[7] = ioj::ui::entity_overlay::ObjectiveRole::Defend;
    objective_roles[8] = ioj::ui::entity_overlay::ObjectiveRole::Destroy;
    auto& frame{frame_store->next()};
    static_cast<void>(ioj::presentation::collect_entity_overlay_instances(
        make_view(entities, health_table),
        entity_type_radii,
        {objective_roles.GetData(), static_cast<std::size_t>(objective_roles.Num())},
        make_team_colours(),
        {100, 100, 100},
        {},
        10000.0f,
        collected));
    frame.instances.Append(collected.data(), static_cast<int32>(collected.size()));
    frame_store->publish();
    auto const before_path{FPaths::Combine(output_directory, TEXT("camera_plane_before.png"))};
    if (!write_entity_overlay_debug_image(frame_store, view, before_path)) {
        return false;
    }

    ::ioj::set_vector(entities.get_view().view_locations(), count - 1, {100.0f, 50.0f, 50.0f});
    auto& moved_frame{frame_store->next()};
    static_cast<void>(ioj::presentation::collect_entity_overlay_instances(
        make_view(entities, health_table),
        entity_type_radii,
        {objective_roles.GetData(), static_cast<std::size_t>(objective_roles.Num())},
        make_team_colours(),
        {100, 100, 100},
        {},
        10000.0f,
        collected));
    moved_frame.instances.Append(collected.data(), static_cast<int32>(collected.size()));
    frame_store->publish();
    auto const after_path{FPaths::Combine(output_directory, TEXT("camera_plane_after.png"))};
    if (!write_entity_overlay_debug_image(frame_store, view, after_path)) {
        return false;
    }

    return true;
}
} // namespace

UEntityOverlayBenchmarkCommandlet::UEntityOverlayBenchmarkCommandlet() {
    IsClient = true;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = false;
}

int32 UEntityOverlayBenchmarkCommandlet::Main(FString const& params) {
    if (GUsingNullRHI) {
        UE_LOG(LogEntityOverlayBenchmark,
               Error,
               TEXT("EntityOverlayBenchmark requires -RenderOffscreen, not -NullRHI."));
        return 1;
    }

    int32 warmup{3};
    int32 iterations{20};
    FParse::Value(*params, TEXT("Warmup="), warmup);
    FParse::Value(*params, TEXT("Iterations="), iterations);
    if (warmup < 0 || iterations <= 0) {
        UE_LOG(LogEntityOverlayBenchmark, Error, TEXT("Invalid benchmark iteration counts."));
        return 1;
    }

    FString output_path;
    if (!FParse::Value(*params, TEXT("Output="), output_path)) {
        output_path = FPaths::Combine(FPaths::ProjectSavedDir(),
                                      TEXT("Benchmarks/EntityOverlayBenchmark.csv"));
    }
    auto const report{run_entity_overlay_benchmark(warmup, iterations)};
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(output_path), true);
    if (!FFileHelper::SaveStringToFile(report.to_csv(), *output_path)) {
        UE_LOG(LogEntityOverlayBenchmark, Error, TEXT("Could not write %s"), *output_path);
        return 1;
    }

    UE_LOG(LogEntityOverlayBenchmark, Display, TEXT("\n%s"), *report.to_text());
    auto const debug_output{
        FPaths::Combine(FPaths::GetPath(output_path), TEXT("EntityOverlayVisual"))};
    if (!write_debug_frames(debug_output)) {
        UE_LOG(LogEntityOverlayBenchmark, Error, TEXT("Could not write visual debug frames."));
        return 1;
    }
    UE_LOG(LogEntityOverlayBenchmark, Display, TEXT("Wrote %s"), *output_path);
    return 0;
}
