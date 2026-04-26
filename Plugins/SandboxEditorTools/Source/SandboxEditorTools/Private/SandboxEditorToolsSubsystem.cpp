#include "SandboxEditorToolsSubsystem.h"

#include "Bool3.h"
#include "Cursor.h"
#include "SandboxEditorToolsLogCategories.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Selection.h"

void USandboxEditorToolsSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);

    cursor = nullptr;

    FEditorDelegates::OnMapOpened.AddUObject(this, &ThisClass::on_map_opened);
}
void USandboxEditorToolsSubsystem::Deinitialize() {
    FEditorDelegates::OnMapOpened.RemoveAll(this);

    Super::Deinitialize();
}

auto USandboxEditorToolsSubsystem::get_cursor() -> TWeakObjectPtr<ACursor> {
    return ensure_cursor_exists();
}
void USandboxEditorToolsSubsystem::move_cursor_to_actor(AActor* actor) {
    ensure_cursor_exists();
    if (!cursor.IsValid()) {
        return;
    }
    cursor->SetActorLocation(actor->GetActorLocation());
}
void USandboxEditorToolsSubsystem::destroy_cursor() {
    if (!cursor.IsValid()) {
        return;
    }
    cursor->Destroy();
    cursor = nullptr;
    return;
}
auto USandboxEditorToolsSubsystem::ensure_cursor_exists() -> TWeakObjectPtr<ACursor> {
    if (!cursor.IsValid()) {
        spawn_cursor();
    }

    if (!cursor.IsValid()) {
        UE_LOG(LogSandboxEditorTools, Warning, TEXT("Failed to spawn cursor."));
    } else {
        cursor->SetFolderPath(TEXT("_Editor"));
    }

    return cursor;
}
void USandboxEditorToolsSubsystem::spawn_cursor() {
    UE_LOG(LogSandboxEditorTools, Verbose, TEXT("spawn_cursor"));

    FActorSpawnParameters spawn_params{};

    auto* world{GEditor->GetEditorWorldContext().World()};
    if (!world) {
        UE_LOG(LogSandboxEditorTools, Warning, TEXT("World is nullptr"));
        return;
    }

    UE_LOG(LogSandboxEditorTools, Verbose, TEXT("Spawning cursor"));
    cursor = world->SpawnActor<ACursor>(spawn_params);
}

void USandboxEditorToolsSubsystem::align_actors_to_cursor(FRotationBool axes) {
    UE_LOG(LogSandboxEditorTools, Verbose, TEXT("align_actors_to_cursor"));

    if (ensure_cursor_exists() == nullptr) {
        return;
    }

    auto selected_actors{get_selected_actors()};

    for (auto* actor : selected_actors) {
        align_actor_to(*actor, *cursor, axes);
    }
}
void USandboxEditorToolsSubsystem::align_actor_to(AActor& actor,
                                                  AActor const& ref,
                                                  FRotationBool axes) {
    UE_LOG(LogSandboxEditorTools,
           Verbose,
           TEXT("Aligning: %s to %s"),
           *actor.GetName(),
           *ref.GetName());

    auto const from{actor.GetActorLocation()};
    auto const original_rotation{actor.GetActorRotation()};
    auto const to{ref.GetActorLocation()};
    auto look_at_rotation{(to - from).Rotation()};

    UE_LOG(LogSandboxEditorTools,
           Verbose,
           TEXT("Axes bools: Roll=%d Pitch=%d Yaw=%d"),
           axes.roll,
           axes.pitch,
           axes.yaw);

    if (!axes.roll) {
        look_at_rotation.Roll = original_rotation.Roll;
    }
    if (!axes.pitch) {
        look_at_rotation.Pitch = original_rotation.Pitch;
    }
    if (!axes.yaw) {
        look_at_rotation.Yaw = original_rotation.Yaw;
    }

    actor.SetActorRotation(look_at_rotation);
}
void USandboxEditorToolsSubsystem::position_actors(FLayoutSettings const settings) {
    UE_LOG(LogSandboxEditorTools, Verbose, TEXT("position_actors"));

    if (ensure_cursor_exists() == nullptr) {
        return;
    }

    auto actors{get_selected_actors()};

    auto const n{actors.Num()};
    if (!n) {
        return;
    }

    auto const side{FMath::CeilToInt(FMath::Pow(static_cast<double>(n), 1.0 / 3.0))};

    auto const origin{cursor->GetActorLocation()};

    for (int32 i{0}; i < n; ++i) {
        auto const x{i % side};
        auto const y{(i / side) % side};
        auto const z{i / (side * side)};

        auto const pos{
            origin + FVector{x * settings.offset.X, y * settings.offset.Y, z * settings.offset.Z}};

        if (IsValid(actors[i])) {
            actors[i]->SetActorLocation(pos);
        }
    }
}

auto USandboxEditorToolsSubsystem::get_selected_actors() -> TArray<AActor*> {
    UE_LOG(LogSandboxEditorTools, Verbose, TEXT("get_selected_actors"));

    TArray<AActor*> actors{};

    if (!GEditor) {
        UE_LOG(LogSandboxEditorTools, Warning, TEXT("GEditor is nullptr."));
        return actors;
    }

    auto* selected_actors{GEditor->GetSelectedActors()};

    if (!selected_actors) {
        UE_LOG(LogSandboxEditorTools, Display, TEXT("selected_actors is nullptr."));
        return actors;
    }

    actors.Reserve(selected_actors->Num());
    for (FSelectionIterator it(*selected_actors); it; ++it) {
        if (auto* actor{Cast<AActor>(*it)}; IsValid(actor)) {
            actors.Add(actor);
        }
    }

    return actors;
}
void USandboxEditorToolsSubsystem::on_map_opened(FString const&, bool) {
    UE_LOG(LogSandboxEditorTools, Verbose, TEXT("on_map_opened"));

    cursor = nullptr;

    if (!GEditor) {
        UE_LOG(LogSandboxEditorTools, Verbose, TEXT("GEditor is nullptr."));
        return;
    }

    auto* world{GEditor->GetEditorWorldContext().World()};
    if (world) {
        for (auto it{TActorIterator<ACursor>(world)}; it; ++it) {
            if (it) {
                cursor = *it;
                break;
            }
        }
    }
}
