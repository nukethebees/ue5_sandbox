#pragma once

#include "Blueprint/UserWidget.h"

#include "LevelOutcomeRowWidget.generated.h"

class UButton;
class UTextBlock;

namespace ml::ioj {
struct FLevelOutcomeSummary;

DECLARE_MULTICAST_DELEGATE_OneParam(FLevelOutcomeRowSelected, FString const&);

UCLASS()
class SPACEGAME_API ULevelOutcomeRowWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_outcome(FLevelOutcomeSummary const& outcome);
    void set_selected(bool selected);

    FLevelOutcomeRowSelected selected;
  protected:
    void NativeOnInitialized() override;

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UButton* row_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* display_name_text{nullptr};
  private:
    UFUNCTION()
    void handle_clicked();

    FString outcome_id_{};
};
}
