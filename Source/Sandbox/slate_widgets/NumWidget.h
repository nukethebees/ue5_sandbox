// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Components/Widget.h"

#include "NumWidget.generated.h"

class SANDBOX_API SNumWidget : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SNumWidget) {}
    SLATE_ARGUMENT(FText, label)
    SLATE_END_ARGS()

    void Construct(FArguments const& InArgs);

    FText label;
};

UCLASS()
class UNumWidget : public UWidget {
    GENERATED_BODY()
  public:
  protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
  private:
    TSharedPtr<SNumWidget> slate_widget;
};
