#include "SandboxEditor/Commandlets/EntityOverlayBenchmarkCommandlet.h"

#include "SandboxUI/EntityOverlay/EntityOverlayBenchmark.h"
#include "SpaceGame/entities/TestEntityRegistry.h"
#include "SpaceGame/presentation/EntityOverlaySource.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RHIGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogEntityOverlayBenchmark, Log, All);

namespace {
auto make_view(FTestEntityRegistry::EntityData const& entities)
    -> FTestEntityRegistry::EntityData::ConstView {
    return {.locations = {entities.locations.xs, entities.locations.ys, entities.locations.zs},
            .velocities = {entities.velocities.xs, entities.velocities.ys, entities.velocities.zs},
            .radii = entities.radii,
            .healths = entities.healths,
            .teams = entities.teams,
            .entity_types = entities.entity_types,
            .alive = entities.alive};
}

auto write_debug_frames(FString const& output_directory) -> bool {
    FTestEntityRegistry registry;
    FTestEntityRegistry::EntityData entities;
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
    int32 const health[]{0, 25, 50, 75, 100, 75, 100, 100, 100, 50};
    float const radii[]{
        50.0f, 100.0f, 200.0f, 400.0f, 1000.0f, 200.0f, 200.0f, 200.0f, 200.0f, 400.0f};
    auto const count{UE_ARRAY_COUNT(positions)};
    for (int32 index{0}; index < count; ++index) {
        entities.locations.add(positions[index]);
        entities.velocities.add(FVector3f::ZeroVector);
        entities.radii.Add(radii[index]);
        entities.healths.Add(health[index]);
        entities.teams.Add(ETestTeam::White);
        entities.entity_types.Add(ETestEntityType::Turret);
        entities.alive.Add(1);
    }
    static_cast<void>(registry.add_entities(make_view(entities)));

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

    FEntityOverlayCollector collector;
    auto frame_store{MakeShared<FEntityOverlayFrameStore, ESPMode::ThreadSafe>()};
    auto& frame{frame_store->next()};
    static_cast<void>(collect_entity_overlay_instances(make_view(registry.get_entity_data()),
                                                       {100, 100, 100},
                                                       FVector3f::ZeroVector,
                                                       10000.0f,
                                                       frame.instances,
                                                       collector));
    frame_store->publish();
    auto const before_path{FPaths::Combine(output_directory, TEXT("camera_plane_before.png"))};
    if (!write_entity_overlay_debug_image(frame_store, view, before_path)) {
        return false;
    }

    entities.locations.set(count - 1, {100.0f, 50.0f, 50.0f});
    FTestEntityRegistry moved_registry;
    static_cast<void>(moved_registry.add_entities(make_view(entities)));
    auto& moved_frame{frame_store->next()};
    static_cast<void>(collect_entity_overlay_instances(make_view(moved_registry.get_entity_data()),
                                                       {100, 100, 100},
                                                       FVector3f::ZeroVector,
                                                       10000.0f,
                                                       moved_frame.instances,
                                                       collector));
    frame_store->publish();
    auto const after_path{FPaths::Combine(output_directory, TEXT("camera_plane_after.png"))};
    return write_entity_overlay_debug_image(frame_store, view, after_path);
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
