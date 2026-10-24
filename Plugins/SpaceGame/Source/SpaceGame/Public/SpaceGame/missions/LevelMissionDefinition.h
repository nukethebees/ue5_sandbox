#pragma once
#include <ioj/levels/mission_definition.h>

#include <GameFramework/Actor.h>

#include "LevelMissionDefinition.generated.h"

// Proxy maps may bind a programmatically authored mission. Author objective graphs in S7.
USTRUCT()
struct SPACEGAME_API FLevelMissionDefinition {
    GENERATED_BODY()
    void replace_startup_actor(AActor const* old_actor, AActor& new_actor);
    void set_save_mission_results(bool value) { save_mission_results = value; }
    void set_level_identity(FName id, FString name) {
        level_id = id;
        level_display_name = MoveTemp(name);
    }
    ioj::levels::MissionDefinition objectives;
    UPROPERTY()
    TMap<FName, TObjectPtr<AActor>> entity_bindings;
    UPROPERTY(EditAnywhere, Category = "Mission")
    bool save_mission_results{true};
    FName level_id{NAME_None};
    FString level_display_name;
};
