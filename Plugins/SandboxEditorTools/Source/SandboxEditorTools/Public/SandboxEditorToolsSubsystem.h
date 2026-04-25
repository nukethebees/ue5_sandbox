#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"

#include "SandboxEditorToolsSubsystem.generated.h"

class FSubsystemCollectionBase;
class UWorld;

class ACursor;

struct FRotationBool;

UCLASS()
class USandboxEditorToolsSubsystem : public UEditorSubsystem {
  public:
    GENERATED_BODY()

    USandboxEditorToolsSubsystem() = default;

    void Initialize(FSubsystemCollectionBase& Collection) override;
    void Deinitialize() override;

    void move_cursor_to_actor(AActor* actor);
    auto get_cursor() -> TWeakObjectPtr<ACursor>;
    void destroy_cursor();

    void align_actors_to_cursor(FRotationBool axes);
  protected:
    void spawn_cursor();
    auto ensure_cursor_exists() -> TWeakObjectPtr<ACursor>;
    void align_actor_to(AActor& actor, AActor const& ref, FRotationBool axes);

    void on_map_opened(FString const&, bool);

    UPROPERTY(EditAnywhere, Transient)
    TWeakObjectPtr<ACursor> cursor{nullptr};
};
