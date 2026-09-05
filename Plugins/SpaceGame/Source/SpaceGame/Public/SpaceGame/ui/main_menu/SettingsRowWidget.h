#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "SpaceGame/settings/GameSettingsBackend.h"

#include "SettingsRowWidget.generated.h"

class UCheckBox;
class USpinBox;
class UTextBlock;

namespace ml::ioj {

class UGameSettingsSubsystem;

UCLASS()
class USettingsComboBoxString final : public UComboBoxString {
    GENERATED_BODY()
  public:
    void initialize_foreground_color(FSlateColor color);
};

UCLASS()
class SPACEGAME_API USettingsRowWidget final : public UUserWidget {
    GENERATED_BODY()
  public:
    void configure(UGameSettingsSubsystem& settings, FGameSettingDescriptor const& descriptor);
    void refresh();
  protected:
    virtual void NativeOnInitialized() override;
  private:
    UFUNCTION()
    void handle_checked(bool checked);

    UFUNCTION()
    void handle_selection_changed(FString selected_item, ESelectInfo::Type selection_type);

    UFUNCTION()
    void handle_numeric_changed(float value);

    void build_row();
    auto option_index(FGameSettingValue const& value) const -> int32;

    UPROPERTY(Transient)
    UGameSettingsSubsystem* settings_{nullptr};

    FGameSettingDescriptor const* descriptor_{nullptr};
    TArray<FGameSettingOption> options_;

    UPROPERTY(Transient)
    UTextBlock* label_{nullptr};

    UPROPERTY(Transient)
    UCheckBox* check_box_{nullptr};

    UPROPERTY(Transient)
    USettingsComboBoxString* combo_box_{nullptr};

    UPROPERTY(Transient)
    USpinBox* spin_box_{nullptr};

    bool refreshing_{};
    bool built_{};
};

} // namespace ml::ioj
