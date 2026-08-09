#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

#include "WidgetBlueprintGeneratorTestWidgets.generated.h"

UCLASS()
class UWidgetBlueprintGeneratorDefaultRootTestWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> text_widget{nullptr};
};

UCLASS()
class UWidgetBlueprintGeneratorPanelRootTestWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    TObjectPtr<UVerticalBox> root_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> text_widget{nullptr};
};

UCLASS()
class UWidgetBlueprintGeneratorNonPanelRootTestWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    TObjectPtr<UTextBlock> root_widget{nullptr};
};

UCLASS()
class UWidgetBlueprintGeneratorMultipleRootTestWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    TObjectPtr<UVerticalBox> first_root_widget{nullptr};

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    TObjectPtr<UHorizontalBox> second_root_widget{nullptr};
};
