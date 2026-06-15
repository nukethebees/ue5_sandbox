// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <SandboxCore/numeric.h>

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"

#include <concepts>

#include "ValueWidget.generated.h"

UCLASS()
class SANDBOX_API UValueWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void NativeConstruct() override;

    template <ml::Numeric... Ts>
    void update(Ts const... values) {
        if (!value_text) {
            return;
        }

        auto const display{FText::Format(format_spec_text, FText::AsNumber(values)...)};

        value_text->SetText(display);
    }
    template <ml::Numeric... Ts>
    void update(FNumberFormattingOptions const& options, Ts const... values) {
        if (!value_text) {
            return;
        }

        auto const display{FText::Format(format_spec_text, FText::AsNumber(values, &options)...)};

        value_text->SetText(display);
    }

    void update(FStringView const value) {
        if (value_text) {
            auto const display{FText::Format(format_spec_text, FText::FromStringView(value))};
            value_text->SetText(display);
        }
    }
  protected:
    UPROPERTY(meta = (BindWidget))
    UTextBlock* value_text;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    FName format_spec;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    int32 font_size{24};
  private:
    FText format_spec_text;
};
