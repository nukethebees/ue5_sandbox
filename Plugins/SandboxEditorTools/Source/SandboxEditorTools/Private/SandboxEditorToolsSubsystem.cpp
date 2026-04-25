#include "SandboxEditorToolsSubsystem.h"

#include "Cursor.h"
#include "SandboxEditorToolsLogCategories.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void USandboxEditorToolsSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);

    FEditorDelegates::OnMapOpened.AddUObject(this, &ThisClass::on_map_opened);
}
void USandboxEditorToolsSubsystem::Deinitialize() {
    FEditorDelegates::OnMapOpened.RemoveAll(this);

    Super::Deinitialize();
}

auto USandboxEditorToolsSubsystem::get_cursor() -> ACursor* {
    return ensure_cursor_exists();
}
void USandboxEditorToolsSubsystem::move_cursor_to_actor(AActor* actor) {
    ensure_cursor_exists();
    if (!cursor) {
        return;
    }
    cursor->SetActorLocation(actor->GetActorLocation());
}
void USandboxEditorToolsSubsystem::destroy_cursor() {
    if (!cursor) {
        return;
    }
    cursor->Destroy();
    cursor = nullptr;
    return;
}
auto USandboxEditorToolsSubsystem::ensure_cursor_exists() -> ACursor* {
    if (!cursor) {
        spawn_cursor();
    }

    if (!cursor) {
        UE_LOG(LogSandboxEditorTools, Warning, TEXT("Failed to spawn cursor."));
    } else {
        cursor->SetFolderPath(TEXT("_Editor"));
    }

    return cursor;
}
void USandboxEditorToolsSubsystem::spawn_cursor() {
    FActorSpawnParameters spawn_params{};

    auto* world{GEditor->GetEditorWorldContext().World()};
    if (!world) {
        UE_LOG(LogSandboxEditorTools, Warning, TEXT("World is nullptr"));
        return;
    }

    cursor = world->SpawnActor<ACursor>(spawn_params);
}

void USandboxEditorToolsSubsystem::on_map_opened(FString const&, bool) {
    UE_LOG(LogSandboxEditorTools, Verbose, TEXT("on_map_opened"));
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
