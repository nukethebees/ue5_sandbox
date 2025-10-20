// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/enums/TeamID.h"
#include "Sandbox/mixins/LogMsgMixin.hpp"

#include "PawnSpawner.generated.h"

class AAIController;

UCLASS()
class SANDBOX_API APawnSpawner
    : public AActor
    , public ml::LogMsgMixin<"APawnSpawner"> {
    GENERATED_BODY()
  public:
    APawnSpawner();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    TSubclassOf<APawn> pawn_class;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    TSubclassOf<AAIController> ai_controller_class;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    float pawns_per_second{1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    bool spawn_at_start{true};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    bool enabled{true};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    ETeamID spawned_pawn_team_id{ETeamID::Enemy};
  protected:
    virtual void BeginPlay() override;
  public:
    virtual void Tick(float DeltaTime) override;
  private:
    float time_since_last_spawn{0.0f};
    void spawn_pawn();
};
