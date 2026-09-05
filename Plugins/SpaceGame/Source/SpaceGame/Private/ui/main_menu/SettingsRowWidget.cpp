#include "SpaceGame/ui/main_menu/SettingsRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "SpaceGame/settings/GameSettingsSubsystem.h"

namespace ml::ioj {

void USettingsComboBoxString::initialize_foreground_color(FSlateColor const color) {
    InitForegroundColor(color);
}

void USettingsRowWidget::configure(UGameSettingsSubsystem& settings,
                                   FGameSettingDescriptor const& descriptor) {
    settings_ = &settings;
    descriptor_ = &descriptor;
    build_row();
    refresh();
}

void USettingsRowWidget::refresh() {
    if (!built_ || !IsValid(settings_) || descriptor_ == nullptr) {
        return;
    }

    refreshing_ = true;
    SetIsEnabled(settings_->is_available(descriptor_->id));
    auto const value{settings_->value(descriptor_->id)};
    if (IsValid(check_box_)) {
        auto const* checked{std::get_if<bool>(&value)};
        check_box_->SetIsChecked(checked != nullptr && *checked);
    }
    if (IsValid(combo_box_)) {
        auto const index{option_index(value)};
        if (options_.IsValidIndex(index)) {
            combo_box_->SetSelectedIndex(index);
        } else {
            combo_box_->ClearSelection();
        }
    }
    if (IsValid(spin_box_)) {
        if (auto const* float_value{std::get_if<float>(&value)}) {
            spin_box_->SetValue(*float_value);
        } else if (auto const* integer_value{std::get_if<int32>(&value)}) {
            spin_box_->SetValue(static_cast<float>(*integer_value));
        }
    }
    refreshing_ = false;
}

void USettingsRowWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();
    build_row();
    refresh();
}

void USettingsRowWidget::handle_checked(bool const checked) {
    if (!refreshing_ && IsValid(settings_) && descriptor_ != nullptr) {
        settings_->set_setting(descriptor_->id, FGameSettingValue{checked});
    }
}

void USettingsRowWidget::handle_selection_changed(FString const selected_item,
                                                  ESelectInfo::Type const selection_type) {
    static_cast<void>(selected_item);
    static_cast<void>(selection_type);
    if (refreshing_ || !IsValid(settings_) || descriptor_ == nullptr || !IsValid(combo_box_)) {
        return;
    }
    auto const index{combo_box_->GetSelectedIndex()};
    if (options_.IsValidIndex(index)) {
        settings_->set_setting(descriptor_->id, options_[index].value);
    }
}

void USettingsRowWidget::handle_numeric_changed(float const value) {
    if (refreshing_ || !IsValid(settings_) || descriptor_ == nullptr) {
        return;
    }
    if (descriptor_->control_kind == ESettingControlKind::IntegerRange) {
        settings_->set_setting(descriptor_->id, FGameSettingValue{FMath::RoundToInt32(value)});
    } else {
        settings_->set_setting(descriptor_->id, FGameSettingValue{value});
    }
}

void USettingsRowWidget::build_row() {
    if (built_ || descriptor_ == nullptr || WidgetTree == nullptr) {
        return;
    }
    built_ = true;

    auto* root{
        WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("root"))};
    WidgetTree->RootWidget = root;

    auto* label_size{
        WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("label_size"))};
    label_size->SetWidthOverride(300.0f);
    label_ = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("label"));
    label_->SetText(descriptor_->label);
    label_->SetToolTipText(descriptor_->tooltip);
    label_size->AddChild(label_);
    root->AddChildToHorizontalBox(label_size);

    switch (descriptor_->control_kind) {
        case ESettingControlKind::Toggle: {
            check_box_ = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(),
                                                                TEXT("value_toggle"));
            check_box_->OnCheckStateChanged.AddDynamic(this, &ThisClass::handle_checked);
            root->AddChildToHorizontalBox(check_box_);
            break;
        }
        case ESettingControlKind::Choice: {
            combo_box_ = WidgetTree->ConstructWidget<USettingsComboBoxString>(
                USettingsComboBoxString::StaticClass(), TEXT("value_choice"));
            auto const option_text_color{FSlateColor{FLinearColor::White}};
            combo_box_->initialize_foreground_color(option_text_color);
            auto item_style{combo_box_->GetItemStyle()};
            item_style.SetTextColor(option_text_color);
            item_style.SetSelectedTextColor(option_text_color);
            combo_box_->SetItemStyle(item_style);
            options_ = settings_->options(descriptor_->id);
            for (auto const& option : options_) {
                combo_box_->AddOption(option.label.ToString());
            }
            combo_box_->OnSelectionChanged.AddDynamic(this, &ThisClass::handle_selection_changed);
            root->AddChildToHorizontalBox(combo_box_);
            break;
        }
        case ESettingControlKind::FloatRange:
        case ESettingControlKind::IntegerRange: {
            spin_box_ = WidgetTree->ConstructWidget<USpinBox>(USpinBox::StaticClass(),
                                                              TEXT("value_number"));
            spin_box_->SetMinValue(static_cast<float>(descriptor_->minimum));
            spin_box_->SetMaxValue(static_cast<float>(descriptor_->maximum));
            spin_box_->SetMinSliderValue(static_cast<float>(descriptor_->minimum));
            spin_box_->SetMaxSliderValue(static_cast<float>(descriptor_->maximum));
            spin_box_->SetDelta(static_cast<float>(descriptor_->step));
            spin_box_->OnValueChanged.AddDynamic(this, &ThisClass::handle_numeric_changed);
            root->AddChildToHorizontalBox(spin_box_);
            break;
        }
        case ESettingControlKind::Custom: {
            UE_LOG(LogTemp,
                   Warning,
                   TEXT("No custom widget factory is registered for game setting %s"),
                   descriptor_->name);
            break;
        }
    }
}

auto USettingsRowWidget::option_index(FGameSettingValue const& value) const -> int32 {
    return options_.IndexOfByPredicate(
        [&value](FGameSettingOption const& option) { return option.value == value; });
}

} // namespace ml::ioj
