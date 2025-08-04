// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "CoreMinimal.h"

#include "JumpWidget.generated.h"

UCLASS()
class SANDBOX_API UJumpWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UFUNCTION(BlueprintCallable)
    void update_jump(int new_jump);
  protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* jump_text;
};
