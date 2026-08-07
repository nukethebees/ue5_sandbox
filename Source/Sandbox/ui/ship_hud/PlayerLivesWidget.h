#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "PlayerLivesWidget.generated.h"

class UValueWidget;

UCLASS()
class SANDBOX_API UPlayerLivesWidget : public UUserWidget {
  public:
    GENERATED_BODY()

    void set_value(int32 value);
    void set_font_size(int32 const new_font_size);
    auto get_font_size() const noexcept -> int32;
  protected:
    UPROPERTY(meta = (BindWidget))
    UValueWidget* widget{nullptr};
};
