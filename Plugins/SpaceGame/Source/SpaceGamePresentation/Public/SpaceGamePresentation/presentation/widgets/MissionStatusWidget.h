#pragma once

#include <SpaceGamePresentation/ui/style/GameUiStyle.h>

#include <Blueprint/UserWidget.h>
#include <CoreMinimal.h>

#include "MissionStatusWidget.generated.h"

class UValueWidget;

namespace ioj::hud_manager {
struct FMissionDataCache;
}

UCLASS()
class SPACEGAMEPRESENTATION_API UMissionStatusWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_mission_data(ioj::hud_manager::FMissionDataCache const& data);
    void apply_hud_style(ioj::FGameHudStyle const& style);
    void set_font_size(int32 const new_font_size);
    auto get_font_size() const noexcept -> int32 { return font_size; }
  protected:
    void NativeConstruct() override;
    void NativePreConstruct() override;
    auto RebuildWidget() -> TSharedRef<SWidget> override;

    UPROPERTY(meta = (BindWidget))
    UValueWidget* mission_mode_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UValueWidget* mission_time_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UValueWidget* enemies_remaining_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UValueWidget* time_remaining_widget{nullptr};

    UPROPERTY(EditAnywhere, Category = "UI")
    int32 font_size{24};
  private:
    auto check_widget_bindings() const -> bool;
    TOptional<ioj::FGameHudStyle> hud_style_{};
};
