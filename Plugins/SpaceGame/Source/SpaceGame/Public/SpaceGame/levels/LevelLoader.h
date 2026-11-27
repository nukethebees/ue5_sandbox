#pragma once

#include <ioj/levels/control_context.h>
#include <ioj/presentation/level_frame.h>

#include <CoreMinimal.h>

#include <expected>
#include <optional>

class UWorld;

namespace ioj {
class ASpaceGamePlayerController;
class ATestSpaceShip;
class USpaceGameLevelConfig;

enum class ELevelLoadErrorCode : uint8 {
    InvalidDefinition,
    InvalidWorld,
    InvalidLevelState,
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

using FLevelLoadResult = std::expected<void, FLevelLoadErrors>;

struct FLevelVisualLoad {
    ASpaceGamePlayerController& controller;
    levels::ControlContext playerless_control_context;
};

struct FLevelActorLoadRequest {
    UWorld& world;
    USpaceGameLevelConfig const& config;
    std::optional<presentation::PlayerFrame> const& player_frame;
    std::optional<Transform3d> const& camera;
    TObjectPtr<ATestSpaceShip>& player;
    std::optional<FLevelVisualLoad> visual;
};

class SPACEGAME_API FLevelLoader {
  public:
    static auto load(FLevelActorLoadRequest const& request) -> FLevelLoadResult;
    static void report_failure(UWorld* world, FString message);
};
}
