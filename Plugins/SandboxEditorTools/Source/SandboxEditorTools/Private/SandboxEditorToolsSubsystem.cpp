#include "SandboxEditorToolsSubsystem.h"

#include "Bool3.h"
#include "Cursor.h"
#include "SandboxEditorToolsLogCategories.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Selection.h"

// Setup
void USandboxEditorToolsSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);

    cursor = nullptr;

    FEditorDelegates::OnMapOpened.AddUObject(this, &ThisClass::on_map_opened);
}
void USandboxEditorToolsSubsystem::Deinitialize() {
    FEditorDelegates::OnMapOpened.RemoveAll(this);

    Super::Deinitialize();
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

// Cursor
auto USandboxEditorToolsSubsystem::get_cursor() -> TWeakObjectPtr<ACursor> {
    return ensure_cursor_exists();
}
auto USandboxEditorToolsSubsystem::get_cursor_name() const -> FString {
    if (cursor.IsValid()) {
        return cursor.Get()->GetName();
    } else {
        return TEXT("<invalid ptr>");
    }
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

// Alignment
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
void USandboxEditorToolsSubsystem::align_actors_away_from_cursor(FRotationBool axes) {
    UE_LOG(LogSandboxEditorTools, Verbose, TEXT("align_actors_away_from_cursor"));

    if (ensure_cursor_exists() == nullptr) {
        return;
    }

    auto selected_actors{get_selected_actors()};

    for (auto* actor : selected_actors) {
        align_actor_away_from(*actor, *cursor, axes);
    }
}
void USandboxEditorToolsSubsystem::align_actor_to(AActor& actor,
                                                  AActor const& ref,
                                                  FRotationBool axes) {
    auto const original_rotation{actor.GetActorRotation()};
    auto look_at_rotation{get_rotation_to(actor, ref)};

    finish_rotation(actor, original_rotation, look_at_rotation, axes);
}
void USandboxEditorToolsSubsystem::align_actor_away_from(AActor& actor,
                                                         AActor const& ref,
                                                         FRotationBool axes) {
    auto const original_rotation{actor.GetActorRotation()};
    auto look_at_rotation{get_rotation_to(ref, actor)};

    finish_rotation(actor, original_rotation, look_at_rotation, axes);
}
auto USandboxEditorToolsSubsystem::get_rotation_to(AActor const& a, AActor const& b) -> FRotator {
    return (b.GetActorLocation() - a.GetActorLocation()).Rotation();
}
void USandboxEditorToolsSubsystem::finish_rotation(AActor& actor,
                                                   FRotator const& original_rot,
                                                   FRotator& new_rot,
                                                   FRotationBool axes) {
    if (!axes.roll) {
        new_rot.Roll = original_rot.Roll;
    }
    if (!axes.pitch) {
        new_rot.Pitch = original_rot.Pitch;
    }
    if (!axes.yaw) {
        new_rot.Yaw = original_rot.Yaw;
    }

    actor.SetActorRotation(new_rot);
}

// Position
void USandboxEditorToolsSubsystem::position_actors(FLayoutSettings const settings) {
    UE_LOG(LogSandboxEditorTools, Verbose, TEXT("position_actors"));

    if (ensure_cursor_exists() == nullptr) {
        return;
    }

    auto actors{get_selected_actors()};
    auto const n{actors.Num()};

    if (n == 0) {
        return;
    }

    auto const origin{cursor->GetActorLocation()};

    auto const use_x{!FMath::IsNearlyZero(settings.offset.X)};
    auto const use_y{!FMath::IsNearlyZero(settings.offset.Y)};
    auto const use_z{!FMath::IsNearlyZero(settings.offset.Z)};

    auto const dims{static_cast<int32>(use_x) + static_cast<int32>(use_y) +
                    static_cast<int32>(use_z)};

    if (dims == 0) {
        for (auto* actor : actors) {
            if (IsValid(actor)) {
                actor->SetActorLocation(origin);
            }
        }

        return;
    }

    auto const side{dims == 1   ? n
                    : dims == 2 ? FMath::CeilToInt(FMath::Sqrt(static_cast<double>(n)))
                                : FMath::CeilToInt(FMath::Pow(static_cast<double>(n), 1.0 / 3.0))};

    for (int32 i{0}; i < n; ++i) {
        auto remaining{i};
        FVector grid{0.0};

        if (use_x) {
            grid.X = remaining % side;
            remaining /= side;
        }

        if (use_y) {
            grid.Y = remaining % side;
            remaining /= side;
        }

        if (use_z) {
            grid.Z = remaining;
        }

        auto const pos{origin + FVector{grid.X * settings.offset.X,
                                        grid.Y * settings.offset.Y,
                                        grid.Z * settings.offset.Z}};

        if (IsValid(actors[i])) {
            actors[i]->SetActorLocation(pos);
        }
    }
}

// Selection
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
