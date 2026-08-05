#include "Sandbox/players/PawnSpawner.h"

#include "AIController.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GenericTeamAgentInterface.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

APawnSpawner::APawnSpawner() {
    PrimaryActorTick.bCanEverTick = true;
}

void APawnSpawner::BeginPlay() {
    Super::BeginPlay();

    if (!enabled) {
        SetActorTickEnabled(false);
        return;
    }

    if (spawn_at_start) {
        spawn_pawn();
    }
}

void APawnSpawner::Tick(float DeltaTime) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::APawnSpawner::Tick"))

    Super::Tick(DeltaTime);

    time_since_last_spawn += DeltaTime;
    auto const seconds_per_pawn{1.0f / pawns_per_second};

    if (time_since_last_spawn >= seconds_per_pawn) {
        spawn_pawn();
        time_since_last_spawn = 0.0f;
    }
}

void APawnSpawner::spawn_pawn() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::APawnSpawner::spawn_pawn"))

    RETURN_IF_NULLPTR(pawn_class);
    RETURN_IF_NULLPTR(ai_controller_class);

    auto const spawn_location{GetActorLocation()};
    auto const spawn_rotation{FRotator::ZeroRotator};

    FActorSpawnParameters spawn_params{};
    spawn_params.Owner = this;

    auto* spawned_pawn{
        GetWorld()->SpawnActor<APawn>(pawn_class, spawn_location, spawn_rotation, spawn_params)};

    RETURN_IF_NULLPTR(spawned_pawn);
    // Set team ID if the pawn implements IGenericTeamAgentInterface
    if (auto* team_agent{Cast<IGenericTeamAgentInterface>(spawned_pawn)}) {
        team_agent->SetGenericTeamId(FGenericTeamId(static_cast<uint8>(spawned_pawn_team_id)));
    }

    spawned_pawn->AIControllerClass = ai_controller_class;
    spawned_pawn->AutoPossessAI = EAutoPossessAI::Spawned;
    spawned_pawn->SpawnDefaultController();

    log_verbose(TEXT("Spawned pawn at location: %s"), *spawn_location.ToString());
}
