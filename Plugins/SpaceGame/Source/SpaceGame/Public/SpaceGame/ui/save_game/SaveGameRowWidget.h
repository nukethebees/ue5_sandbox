#pragma once
#include <ioj/persistence/save_browser.h>

#include "Blueprint/UserWidget.h"

#include "SaveGameRowWidget.generated.h"

class UButton;
class UTextBlock;

namespace ioj {

DECLARE_MULTICAST_DELEGATE_OneParam(FSaveGameRowSelected, FString const&);

UCLASS()
class SPACEGAME_API USaveGameRowWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_summary(persistence::ProfileSummary const& summary);
    void set_selected(bool selected);
    void focus_row();
    [[nodiscard]] auto get_focus_target() const -> UWidget*;

    FSaveGameRowSelected selected;
  protected:
    void NativeOnInitialized() override;

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UButton* row_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* display_name_text{nullptr};
  private:
    UFUNCTION()
    void handle_clicked();

    FString profile_id_{};
};
}
