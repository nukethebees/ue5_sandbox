#pragma once

#include <CoreMinimal.h>

namespace ml {
struct SPACEGAME_API FLevelTeamId {
    FName value{NAME_None};

    auto operator==(FLevelTeamId const&) const -> bool = default;
};

inline auto GetTypeHash(FLevelTeamId const& id) -> uint32 {
    return GetTypeHash(id.value);
}

struct SPACEGAME_API FLevelEntityId {
    FName value{NAME_None};

    auto operator==(FLevelEntityId const&) const -> bool = default;
    auto is_set() const noexcept -> bool { return !value.IsNone(); }
};

inline auto GetTypeHash(FLevelEntityId const& id) -> uint32 {
    return GetTypeHash(id.value);
}

struct SPACEGAME_API FEntityArchetypeId {
    FName value{NAME_None};

    auto operator==(FEntityArchetypeId const&) const -> bool = default;
};

inline auto GetTypeHash(FEntityArchetypeId const& id) -> uint32 {
    return GetTypeHash(id.value);
}

namespace level_teams {
inline FLevelTeamId const white{FName{TEXT("white")}};
inline FLevelTeamId const red{FName{TEXT("red")}};
inline FLevelTeamId const green{FName{TEXT("green")}};
inline FLevelTeamId const blue{FName{TEXT("blue")}};
inline FLevelTeamId const orange{FName{TEXT("orange")}};
inline FLevelTeamId const yellow{FName{TEXT("yellow")}};
}

namespace level_archetypes {
inline FEntityArchetypeId const player_fighter{FName{TEXT("player-fighter")}};
inline FEntityArchetypeId const capital_ship{FName{TEXT("capital-ship")}};
inline FEntityArchetypeId const static_turret{FName{TEXT("static-turret")}};
}

struct SPACEGAME_API FTeamDefinition {
    FLevelTeamId id{};
};

struct SPACEGAME_API FPlayerDefinition {
    FLevelEntityId id{};
    FEntityArchetypeId archetype{};
    FLevelTeamId team{};
    FVector position{FVector::ZeroVector};
    FRotator rotation{FRotator::ZeroRotator};
};

struct SPACEGAME_API FEntitySpawnDefinition {
    FLevelEntityId id{};
    FEntityArchetypeId archetype{};
    FLevelTeamId team{};
    FVector position{FVector::ZeroVector};
    FRotator rotation{FRotator::ZeroRotator};
};

struct SPACEGAME_API FLevelDefinition {
    TOptional<FPlayerDefinition> player{NullOpt};
    TArray<FTeamDefinition> teams{};
    TArray<FEntitySpawnDefinition> entities{};
};

class SPACEGAME_API FLevelBuilder {
  public:
    void set_player(FPlayerDefinition const& player);
    auto add_team(FTeamDefinition const& team) -> FLevelTeamId;
    auto add_entity(FEntitySpawnDefinition const& entity) -> FLevelEntityId;
    auto finish() -> FLevelDefinition;
  private:
    FLevelDefinition definition_{};
};

enum class ELevelValidationErrorCode : uint8 {
    MissingPlayer,
    EmptyTeamId,
    DuplicateTeamId,
    UnsupportedTeamId,
    UnknownTeamReference,
    EmptyArchetypeId,
    UnsupportedArchetype,
    ArchetypeRoleMismatch,
    DuplicateEntityId,
    InvalidPlacement,
};

struct SPACEGAME_API FLevelValidationError {
    ELevelValidationErrorCode code{};
    FString message{};
};

struct SPACEGAME_API FLevelValidationResult {
    TArray<FLevelValidationError> errors{};

    explicit operator bool() const noexcept { return errors.IsEmpty(); }
};

SPACEGAME_API auto validate_level(FLevelDefinition const& definition) -> FLevelValidationResult;
}
