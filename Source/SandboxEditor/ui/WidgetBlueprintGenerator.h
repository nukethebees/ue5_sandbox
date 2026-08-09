#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/DataAsset.h"

#include "WidgetBlueprintGenerator.generated.h"

class UWidgetBlueprint;

USTRUCT()
struct SANDBOXEDITOR_API FWidgetBlueprintGenerationEntry {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Generation")
    bool generate{false};

    UPROPERTY(EditAnywhere, Category = "Generation", meta = (ContentDir))
    FDirectoryPath output_directory;

    UPROPERTY(EditAnywhere, Category = "Generation")
    TSubclassOf<UUserWidget> widget_class;

    UPROPERTY(EditAnywhere, Category = "Generation")
    FName widget_name;

    UPROPERTY(VisibleAnywhere, Transient, Category = "Generation")
    TObjectPtr<UWidgetBlueprint> existing_widget{nullptr};
};

UCLASS(BlueprintType)
class SANDBOXEDITOR_API UWidgetBlueprintGenerationDataAsset : public UDataAsset {
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere, Category = "Generation")
    TArray<FWidgetBlueprintGenerationEntry> widgets;

    UFUNCTION(CallInEditor, Category = "Generation")
    void generate_widgets();

    void PostLoad() override;
    void PostEditChangeProperty(FPropertyChangedEvent& event) override;
  private:
    void fill_empty_output_directories();
    void refresh_existing_widgets();
};
