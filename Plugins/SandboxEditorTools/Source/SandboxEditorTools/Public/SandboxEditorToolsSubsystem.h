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

    void set_cursor_to(AActor* actor);
    auto get_cursor() -> ACursor*;
  protected:
    void spawn_cursor();
    auto ensure_cursor_exists() -> ACursor*;

    UPROPERTY(EditAnywhere)
    ACursor* cursor{nullptr};
};
