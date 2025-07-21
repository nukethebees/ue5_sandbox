// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Clickable.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UClickable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class SANDBOX_API IClickable
{
	GENERATED_BODY()
public:
    virtual void OnClicked() = 0;
};
