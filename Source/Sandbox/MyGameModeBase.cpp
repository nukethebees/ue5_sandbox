// Fill out your copyright notice in the Description page of Project Settings.

#include "MyGameModeBase.h"

void AMyGameModeBase::BeginPlay() {
    Super::BeginPlay();

    print_msg(TEXT("Game mode start"));
    auto* pc{UGameplayStatics::GetPlayerController(this, 0)};
    auto* my_char{
        Cast<AMyCharacter>(UGameplayStatics::GetActorOfClass(this, AMyCharacter::StaticClass()))};

    if (pc && my_char) {
        print_msg("Possessing character");
        pc->Possess(my_char);
    }
}

AMyGameModeBase::AMyGameModeBase() {
    HUDClass = AMyHUD::StaticClass();
}
