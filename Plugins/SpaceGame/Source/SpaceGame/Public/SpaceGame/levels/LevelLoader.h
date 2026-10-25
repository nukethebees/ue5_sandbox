#pragma once

#include <ioj/levels/authoring/level_source.h>
#include <SpaceGame/ships/player/PlayerControlContext.h>

#include <CoreMinimal.h>

class ATestBatchOrchestrator;

namespace ioj {
enum class ELevelLoadErrorCode : uint8 {
    InvalidDefinition,
    InvalidWorld,
    InvalidOrchestratorState,
    InvalidLevelConfig,
    MissingInfrastructure,
    ExistingLevelActors,
    ActorSpawnFailed,
};

struct SPACEGAME_API FLevelLoadError {
    ELevelLoadErrorCode code{};
    FString message{};
};

struct SPACEGAME_API FLevelLoadResult {
    TArray<FLevelLoadError> errors{};

    explicit operator bool() const noexcept { return errors.IsEmpty(); }
};

class SPACEGAME_API FLevelLoader {
  public:
    explicit FLevelLoader(ATestBatchOrchestrator& orchestrator)
        : orchestrator_{orchestrator} {}

    auto load(levels::authoring::LevelSource const& source,
              EPlayerControlContext playerless_control_context =
                  EPlayerControlContext::Observer) const -> FLevelLoadResult;
  private:
    ATestBatchOrchestrator& orchestrator_;
};
}
