#include "SandboxEditorToolsSubsystem.h"

#include "Cursor.h"
#include "SandboxEditorToolsLogCategories.h"

#include "Editor.h"
#include "Engine/World.h"

auto USandboxEditorToolsSubsystem::get_cursor() -> ACursor* {
    return ensure_cursor_exists();
}
void USandboxEditorToolsSubsystem::set_cursor_to(AActor* actor) {
    ensure_cursor_exists();
    if (!cursor) {
        return;
    }
    cursor->SetActorLocation(actor->GetActorLocation());
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
