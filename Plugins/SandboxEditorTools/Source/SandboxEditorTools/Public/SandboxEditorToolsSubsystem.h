#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"

#include "SandboxEditorToolsSubsystem.generated.h"

class ACursor;

UCLASS()
class USandboxEditorToolsSubsystem : public UEditorSubsystem {
  public:
    GENERATED_BODY()

    USandboxEditorToolsSubsystem() = default;

    void set_selected_actor(AActor* actor) { selected_actor = actor; }
    auto get_selected_actor() { return selected_actor; }

    void set_cursor_to(AActor* actor);
  protected:
    void spawn_cursor();

    UPROPERTY(EditAnywhere)
    AActor* selected_actor{nullptr};

    UPROPERTY(EditAnywhere)
    ACursor* cursor{nullptr};
};
