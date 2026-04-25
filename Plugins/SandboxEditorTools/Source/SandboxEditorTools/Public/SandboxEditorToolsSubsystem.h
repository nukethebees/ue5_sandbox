#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"

#include "SandboxEditorToolsSubsystem.generated.h"

class FSubsystemCollectionBase;
class UWorld;

class ACursor;

UCLASS()
class USandboxEditorToolsSubsystem : public UEditorSubsystem {
  public:
    GENERATED_BODY()

    USandboxEditorToolsSubsystem() = default;

    void Initialize(FSubsystemCollectionBase& Collection) override;
    void Deinitialize() override;

    void move_cursor_to_actor(AActor* actor);
    auto get_cursor() -> ACursor*;
    void destroy_cursor();
  protected:
    void spawn_cursor();
    auto ensure_cursor_exists() -> ACursor*;

    void on_map_opened(FString const&, bool);

    UPROPERTY(EditAnywhere)
    ACursor* cursor{nullptr};
};
