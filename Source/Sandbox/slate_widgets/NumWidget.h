// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <optional>
#include <utility>

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Components/Widget.h"
#include "SlateOptMacros.h"

#include "NumWidget.generated.h"

template <typename T>
class SANDBOX_API SNumWidget : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SNumWidget) {}
    SLATE_ARGUMENT(FText, label)
    SLATE_ARGUMENT(T, value)
    SLATE_ARGUMENT(std::optional<T>, max_value)
    SLATE_END_ARGS()

    BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
    void Construct(FArguments const& InArgs) {
        label_ = InArgs._label;
        value_ = InArgs._value;
        max_value_ = InArgs._max_value;

        ChildSlot[SNew(STextBlock).Text(this, &SNumWidget::get_display_text)];
    }
    END_SLATE_FUNCTION_BUILD_OPTIMIZATION

    FText get_display_text() const {
        if (max_value_) {
            return FText::Format(NSLOCTEXT("StatWidget", "ValueWithMax", "{0}: {1} / {2}"),
                                 label_,
                                 FText::AsNumber(value_),
                                 FText::AsNumber(*max_value_));
        }
        return FText::Format(
            NSLOCTEXT("StatWidget", "ValueOnly", "{0}: {1}"), label_, FText::AsNumber(value_));
    }
  private:
    FText label_;
    T value_;
    std::optional<T> max_value_;
};

UCLASS()
class UNumWidget : public UWidget {
    GENERATED_BODY()
  public:
  protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
  private:
    TSharedPtr<SNumWidget<float>> slate_widget;
};
