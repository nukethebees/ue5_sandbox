#pragma once

#include <GameFramework/Actor.h>
#include <SpaceGame/entities/ProxyEntityMap.h>
#include <SpaceGame/missions/TestMissionManager.h>

#include "LevelMissionDefinition.generated.h"

USTRUCT()
struct FTestMissionStartupData {
    GENERATED_BODY()

    void prune_invalid_actors();

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TArray<TObjectPtr<AActor>> hero_entities;

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TArray<TObjectPtr<AActor>> entities_must_survive;

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TArray<TObjectPtr<AActor>> entities_required_to_kill;
};

USTRUCT()
struct SPACEGAME_API FLevelMissionDefinition {
    GENERATED_BODY()

    void replace_startup_actor(AActor const* old_actor, AActor& new_actor);
    void apply(FTestMissionManager& mission,
               FProxyEntityMap const& proxy_entities,
               FTestEntityRegistry const& registry) const;
    void set_mission_mode(ETestMissionMode mode) { mission_mode = mode; }
    void set_target_time(float value) { target_time = value; }
    void set_kill_target(int32 value) { kill_target = value; }
    void set_save_mission_results(bool value) { save_mission_results = value; }
    void set_level_identity(FName id, FString name) {
        level_id = id;
        level_display_name = MoveTemp(name);
    }
    void add_hero_entity(AActor& actor) { startup_data.hero_entities.Add(&actor); }
    void add_entity_that_must_survive(AActor& actor) {
        startup_data.entities_must_survive.Add(&actor);
    }
    void add_entity_required_to_kill(AActor& actor) {
        startup_data.entities_required_to_kill.Add(&actor);
    }

    UPROPERTY(EditAnywhere, Category = "Mission")
    FTestMissionStartupData startup_data;
    UPROPERTY(EditAnywhere, Category = "Mission")
    ETestMissionMode mission_mode{ETestMissionMode::None};
    UPROPERTY(EditAnywhere, Category = "Mission")
    float target_time{60.f};
    UPROPERTY(EditAnywhere, Category = "Mission")
    int32 kill_target{5};
    UPROPERTY(EditAnywhere, Category = "Mission")
    bool save_mission_results{true};
    FName level_id{NAME_None};
    FString level_display_name;
};
