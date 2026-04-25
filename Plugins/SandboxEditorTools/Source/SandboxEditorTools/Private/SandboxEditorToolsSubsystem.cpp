#include "SandboxEditorToolsSubsystem.h"

#include "Cursor.h"
#include "SandboxEditorToolsLogCategories.h"

#include "Editor.h"
#include "Engine/World.h"

void USandboxEditorToolsSubsystem::set_cursor_to(AActor* actor) {
    if (!cursor) {
        spawn_cursor();
    }
    if (!cursor) {
        UE_LOG(LogSandboxEditorTools, Warning, TEXT("Cursor is nullptr."));
        return;
    }

    cursor->SetActorLocation(actor->GetActorLocation());
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
