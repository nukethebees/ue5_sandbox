#pragma once

#include <SpaceGame/system/GameSubsystem.h>

#include <CoreMinimal.h>

#include <expected>
#include <optional>

class ASpaceGamePlayerController;
class ATestSpaceShip;
class USpaceGameLevelConfig;
class UWorld;

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

struct SPACEGAME_API FLevelLoadErrors {
    auto format(FStringView source_path) const -> FString;

    TArray<FLevelLoadError> errors{};
};

struct FLevelVisualLoad {
    ASpaceGamePlayerController& controller;
    EPlayerControlContext playerless_control_context;
};

struct FLevelActorLoadRequest {
    UWorld& world;
    USpaceGameLevelConfig const& config;
    levels::LevelDefinition const& definition;
    TObjectPtr<ATestSpaceShip>& player;
    std::optional<FLevelVisualLoad> visual;
};

class SPACEGAME_API FLevelLoader {
  public:
    static auto take_pending(UGameInstance* game_instance)
        -> std::expected<FPendingLevelDefinition, FString>;
    static auto load(FLevelActorLoadRequest const& request)
        -> std::expected<void, FLevelLoadErrors>;
    static void report_failure(UWorld* world, FString message);
};
}
