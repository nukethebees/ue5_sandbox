#include "MyGameModeBase.h"

#include "Sandbox/macros/null_checks.hpp"

AMyGameModeBase::AMyGameModeBase() {
    ghost_cleanup_component =
        CreateDefaultSubobject<URemoveGhostsOnStartComponent>(TEXT("GhostCleanupComponent"));
}

void AMyGameModeBase::BeginPlay() {
    Super::BeginPlay();
    constexpr auto logger{NestedLogger<"BeginPlay">()};

    logger.log_display(TEXT("Starting!"));

    TRY_INIT_PTR(pc, UGameplayStatics::GetPlayerController(this, 0));
    TRY_INIT_PTR(character_actor,
                 UGameplayStatics::GetActorOfClass(this, AMyCharacter::StaticClass()));
    TRY_INIT_PTR(my_character, Cast<AMyCharacter>(character_actor));
    pc->Possess(my_character);

#if WITH_EDITOR
    pc->ConsoleCommand(TEXT("show collision"));
#endif
}
