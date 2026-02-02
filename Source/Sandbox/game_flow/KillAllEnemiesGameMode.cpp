#include "KillAllEnemiesGameMode.h"

#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

#include "Sandbox/players/npcs/characters/TestEnemy.h"
#include "Sandbox/players/playable/characters/MyCharacter.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

AKillAllEnemiesGameMode::AKillAllEnemiesGameMode() {
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

void AKillAllEnemiesGameMode::InitGame(FString const& MapName,
                                       FString const& Options,
                                       FString& ErrorMessage) {
    Super::InitGame(MapName, Options, ErrorMessage);
    FGenericTeamId::SetAttitudeSolver(&GetTeamAttitude);
}

void AKillAllEnemiesGameMode::BeginPlay() {
    Super::BeginPlay();

    TRY_INIT_PTR(pc, UGameplayStatics::GetPlayerController(this, 0));
    TRY_INIT_PTR(character_actor,
                 UGameplayStatics::GetActorOfClass(this, AMyCharacter::StaticClass()));
    TRY_INIT_PTR(my_character, Cast<AMyCharacter>(character_actor));
    pc->Possess(my_character);

    UE_LOG(LogSandboxCore, Verbose, TEXT("Registering actors."));
    using T = ATestEnemy;
    for (auto it{TActorIterator<ATestEnemy>{GetWorld()}}; it; ++it) {
        enemies_left++;
        it->on_player_killed.AddUObject(this, &AKillAllEnemiesGameMode::handle_enemy_death);
    }
    UE_LOG(LogSandboxCore, Verbose, TEXT("Registered %d actors."), enemies_left);
}

void AKillAllEnemiesGameMode::handle_enemy_death() {
    enemies_left--;

    if (enemies_left <= 0) {
        UGameplayStatics::OpenLevel(GetWorld(), FName("MainMenu"));
    }
}
