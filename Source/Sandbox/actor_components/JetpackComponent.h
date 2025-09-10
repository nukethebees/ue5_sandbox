// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "JetpackComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UJetpackComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UJetpackComponent();
  protected:
    virtual void BeginPlay() override;
};
