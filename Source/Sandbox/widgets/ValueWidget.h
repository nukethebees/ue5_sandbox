// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <concepts>

#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "CoreMinimal.h"

#include "ValueWidget.generated.h"

UCLASS()
class SANDBOX_API UValueWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    virtual void NativeConstruct() override;
    template <typename T>
        requires (std::integral<T> || std::floating_point<T>)
    void update(T value) {
        if (value_text) {
            auto const display{FText::Format(format_spec_text, FText::AsNumber(value))};
            value_text->SetText(display);
        }
    }
    void update(FStringView value) {
        if (value_text) {
            auto const display{FText::Format(format_spec_text, FText::FromStringView(value))};
            value_text->SetText(display);
        }
    }
    UPROPERTY(meta = (BindWidget))
    UTextBlock* value_text;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    FName format_spec;
  private:
    FText format_spec_text;
};
