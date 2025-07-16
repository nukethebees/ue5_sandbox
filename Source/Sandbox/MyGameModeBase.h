// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "MyCharacter.h"

#include "MyGameModeBase.generated.h"

/**
 *
 */
UCLASS()
class SANDBOX_API AMyGameModeBase : public AGameModeBase {
    GENERATED_BODY()
private:
    void print_msg(FString const & msg);
protected:
    virtual void BeginPlay() override;
};
