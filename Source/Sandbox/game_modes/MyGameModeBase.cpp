// Fill out your copyright notice in the Description page of Project Settings.

#include "MyGameModeBase.h"

AMyGameModeBase::AMyGameModeBase() {
    ghost_cleanup_component =
        CreateDefaultSubobject<URemoveGhostsOnStartComponent>(TEXT("GhostCleanupComponent"));
}

void AMyGameModeBase::BeginPlay() {
    Super::BeginPlay();

    if (auto* pc{UGameplayStatics::GetPlayerController(this, 0)}) {
        if (auto* my_char{Cast<AMyCharacter>(
                UGameplayStatics::GetActorOfClass(this, AMyCharacter::StaticClass()))}) {
            pc->Possess(my_char);
        }
    }
}
