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
    void NativePreConstruct() override;
    void NativeConstruct() override;

    void set_format_spec(FName const new_format_spec);

    template <typename... Ts>
    void update(Ts const... values) {
        if (!value_text) {
            return;
        }

        auto const display{FText::Format(format_spec_text, to_text(values)...)};

        value_text->SetText(display);
    }
    template <typename... Ts>
    void update(FNumberFormattingOptions const& options, Ts const... values) {
        if (!value_text) {
            return;
        }

        auto const display{FText::Format(format_spec_text, to_text(values, &options)...)};

        value_text->SetText(display);
    }
  protected:
    UPROPERTY(meta = (BindWidget))
    UTextBlock* value_text;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    FName format_spec;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    int32 font_size{24};
  private:
    template <typename T>
    static auto to_text(T const& value, FNumberFormattingOptions const* const options = nullptr)
        -> FText {
        if constexpr (ml::Numeric<T>) {
            return FText::AsNumber(value, options);
        } else if constexpr (requires { FText::FromStringView(value); }) {
            return FText::FromStringView(value);
        } else {
            static_assert(!sizeof(T), "Unhandled type");
        }
    }

    void update_format_spec_text();

    FText format_spec_text;
};
