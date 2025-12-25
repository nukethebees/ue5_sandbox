#include "MyGameModeBase.h"

#include "Sandbox/core/SandboxDeveloperSettings.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

AMyGameModeBase::AMyGameModeBase()
    : ghost_cleanup_component{
          CreateDefaultSubobject<URemoveGhostsOnStartComponent>(TEXT("GhostCleanupComponent"))} {
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

void AMyGameModeBase::InitGame(FString const& MapName,
                               FString const& Options,
                               FString& ErrorMessage) {
    Super::InitGame(MapName, Options, ErrorMessage);
    constexpr auto logger{NestedLogger<"InitGame">()};

    // Set up team attitude solver for AI perception
    FGenericTeamId::SetAttitudeSolver(&GetTeamAttitude);
    logger.log_verbose(TEXT("Team attitude solver initialized"));
}

void AMyGameModeBase::BeginPlay() {
    Super::BeginPlay();
    constexpr auto logger{NestedLogger<"BeginPlay">()};

    TRY_INIT_PTR(pc, UGameplayStatics::GetPlayerController(this, 0));
    TRY_INIT_PTR(character_actor,
                 UGameplayStatics::GetActorOfClass(this, AMyCharacter::StaticClass()));
    TRY_INIT_PTR(my_character, Cast<AMyCharacter>(character_actor));
    pc->Possess(my_character);

#if WITH_EDITOR
    auto const* settings{GetDefault<USandboxDeveloperSettings>()};
    if (settings->show_collision) {
        pc->ConsoleCommand(TEXT("show collision"));
    }
#endif
}
