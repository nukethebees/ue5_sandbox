#include "Sandbox/levels/KatinaGameMode.h"

#include "EngineUtils.h"
#include "Sandbox/environment/SandboxActorSpawner.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/MothershipBoss.h"
#include "Sandbox/utilities/world.h"

#include "EngineUtils.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

AKatinaGameMode::AKatinaGameMode() {
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

void AKatinaGameMode::InitGame(FString const& MapName,
                               FString const& Options,
                               FString& ErrorMessage) {
    Super::InitGame(MapName, Options, ErrorMessage);
}

void AKatinaGameMode::BeginPlay() {
    Super::BeginPlay();

    TRY_INIT_PTR(world, GetWorld());
    ml::get_first_actor(*world, boss);

    WARN_IF_EXPR_ELSE(boss == nullptr) {
        boss->on_target_reached.BindUObject(this, &ThisClass::on_boss_target_reached);
        on_boss_killed_delegate = boss->on_killed.AddUObject(this, &ThisClass::on_boss_killed);
    }
}
void AKatinaGameMode::EndPlay(EEndPlayReason::Type const reason) {
    Super::EndPlay(reason);

    if (boss) {
        boss->on_target_reached.Unbind();
        boss->get_on_killed_delegate().Remove(on_boss_killed_delegate);
    }
}
void AKatinaGameMode::on_boss_killed(AActor* actor) {
    TRY_INIT_PTR(msb, Cast<AMothershipBoss>(actor));

    UE_LOG(LogSandboxGameMode, Display, TEXT("Boss is killed"));
}
void AKatinaGameMode::on_boss_target_reached() {
    UE_LOG(LogSandboxGameMode, Display, TEXT("Boss reached target; disabling spawners."));

    TRY_INIT_PTR(world, GetWorld());

    for (auto it{TActorIterator<ASandboxActorSpawner>(world)}; it; ++it) {
        it->stop();
    }
}
