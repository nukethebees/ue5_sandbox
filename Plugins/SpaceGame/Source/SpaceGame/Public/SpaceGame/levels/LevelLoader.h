#pragma once

#include <SpaceGame/levels/LevelDefinition.h>

class ATestBatchOrchestrator;

namespace ml {
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
    TArray<FLevelValidationError> validation_errors{};
    TArray<FLevelLoadError> errors{};

    explicit operator bool() const noexcept {
        return validation_errors.IsEmpty() && errors.IsEmpty();
    }
};

class SPACEGAME_API FLevelLoader {
  public:
    explicit FLevelLoader(ATestBatchOrchestrator& orchestrator)
        : orchestrator_{orchestrator} {}

    auto load(FLevelDefinition const& definition) const -> FLevelLoadResult;
  private:
    ATestBatchOrchestrator& orchestrator_;
};
}
