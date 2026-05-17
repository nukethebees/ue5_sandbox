#pragma once

#include "LayoutSettings.h"

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

    // Setup
    void Initialize(FSubsystemCollectionBase& Collection) override;
    void Deinitialize() override;

    // Cursor
    void move_cursor_to_actor(AActor* actor);
    auto get_cursor() -> TWeakObjectPtr<ACursor>;
    auto get_cursor_name() const -> FString;
    void destroy_cursor();

    // Alignment
    void align_actors_to_cursor(FRotationBool axes);
    void align_actors_away_from_cursor(FRotationBool axes);

    // Position
    void position_actors(FLayoutSettings const settings);
  protected:
    // Setup
    void on_map_opened(FString const&, bool);

    // Cursor
    void spawn_cursor();
    auto ensure_cursor_exists() -> TWeakObjectPtr<ACursor>;

    // Alignment
    void align_actor_to(AActor& actor, AActor const& ref, FRotationBool axes);
    void align_actor_away_from(AActor& actor, AActor const& ref, FRotationBool axes);
    static auto get_rotation_to(AActor const& a, AActor const& b) -> FRotator;

    // Position
    void finish_rotation(AActor& actor,
                         FRotator const& original_rot,
                         FRotator& new_rot,
                         FRotationBool axes);

    // Selection
    auto get_selected_actors() -> TArray<AActor*>;

    // Store transient information in the subsystem
    // For permanent info, put it in an actor that you spawn in the level
    // Use a folder like _Editor to hide the instances
    UPROPERTY(EditAnywhere, Transient)
    TWeakObjectPtr<ACursor> cursor{nullptr};
};
