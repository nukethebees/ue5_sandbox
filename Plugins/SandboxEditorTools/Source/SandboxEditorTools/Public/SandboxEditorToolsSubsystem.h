#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"

#include "SandboxEditorToolsSubsystem.generated.h"

UCLASS()
class USandboxEditorToolsSubsystem : public UEditorSubsystem {
  public:
    GENERATED_BODY()

    USandboxEditorToolsSubsystem() = default;

    void set_selected_actor(AActor* actor) { selected_actor = actor; }
    auto get_selected_actor() { return selected_actor; }
  protected:
    UPROPERTY(EditAnywhere)
    AActor* selected_actor{nullptr};
};
