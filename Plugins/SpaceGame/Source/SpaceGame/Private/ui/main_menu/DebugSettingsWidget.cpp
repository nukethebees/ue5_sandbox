#include "SpaceGame/ui/main_menu/DebugSettingsWidget.h"

#include "SandboxUI/widgets/SettingsWidgets.h"
#include "SpaceGame/persistence/SpaceSaveSubsystem.h"
#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGame/ui/common/HiveWidgets.h"
#include "SpaceGame/ui/style/SpaceGameUiTheme.h"

#include <Engine/GameInstance.h>
#include <Widgets/Layout/SBorder.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/Text/STextBlock.h>

namespace ml::ioj {
class SDebugSettingsView final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SDebugSettingsView)
        : _Style(nullptr)
        , _UnlockAllMissions(false)
        , _SettingAvailable(false)
        , _Status() {}
    SLATE_ARGUMENT(FGameUiStyle const*, Style)
    SLATE_ATTRIBUTE(bool, UnlockAllMissions)
    SLATE_ATTRIBUTE(bool, SettingAvailable)
    SLATE_ATTRIBUTE(FText, Status)
    SLATE_EVENT(FOnCheckStateChanged, OnUnlockAllMissionsChanged)
    SLATE_END_ARGS()

    void Construct(FArguments const& args) {
        check(args._Style != nullptr);
        auto const* const style{args._Style};
        auto rows{SNew(SVerticalBox)};
        rows->AddSlot().AutoHeight().Padding(style->settings().row_padding)
            [SAssignNew(unlock_all_missions_, SSettingsToggle)
                 .Style(&style->settings())
                 .Label(NSLOCTEXT("DebugSettings", "UnlockAllMissions", "Unlock All Missions"))
                 .ToolTipText(NSLOCTEXT(
                     "DebugSettings",
                     "UnlockAllMissionsTooltip",
                     "Allows every valid mission to be selected without changing completion "
                     "history or prerequisites."))
                 .Checked(args._UnlockAllMissions)
                 .ControlEnabled(args._SettingAvailable)
                 .OnCheckStateChanged(args._OnUnlockAllMissionsChanged)];

        auto content{
            SNew(SVerticalBox) +
            SVerticalBox::Slot()
                .AutoHeight()[SNew(STextBlock)
                                  .Text(NSLOCTEXT(
                                      "DebugSettings", "Caption", "PROFILE DEBUG SETTINGS"))
                                  .TextStyle(&style->text(EGameTextStyle::Caption))] +
            SVerticalBox::Slot().AutoHeight().Padding(
                FMargin{0.0f, 4.0f})[SNew(STextBlock)
                                         .Text(NSLOCTEXT("DebugSettings", "Title", "DEBUG"))
                                         .TextStyle(&style->text(EGameTextStyle::Heading1))] +
            SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 18.0f, 0.0f, 0.0f})
                [SNew(SSettingsSection)
                     .Style(&style->settings())
                     .Header()[SNew(SHiveSectionHeader)
                                   .Style(style)
                                   .Icon(&style->icon(EGameUiIcon::Gameplay))
                                   .Text(NSLOCTEXT("DebugSettings", "Progression", "PROGRESSION"))]
                     .Title(NSLOCTEXT("DebugSettings", "Progression", "PROGRESSION"))[rows]] +
            SVerticalBox::Slot().FillHeight(1.0f) +
            SVerticalBox::Slot().AutoHeight()[SNew(STextBlock)
                                                  .Text(args._Status)
                                                  .TextStyle(&style->text(EGameTextStyle::Caption))
                                                  .AutoWrapText(true)]};

        ChildSlot[SNew(SBorder)
                      .BorderImage(&style->chrome().body_background)
                      .Padding(style->settings().body_padding)[content]];
    }

    void focus_content() {
        if (unlock_all_missions_.IsValid()) {
            unlock_all_missions_->focus();
        }
    }

    void refresh() { Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint); }
  private:
    TSharedPtr<SSettingsToggle> unlock_all_missions_{};
};

UDebugSettingsWidget::UDebugSettingsWidget(FObjectInitializer const& object_initializer)
    : Super(object_initializer) {
    SetIsFocusable(true);
}

void UDebugSettingsWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    auto* const game_instance{GetGameInstance()};
    game_ = IsValid(game_instance) ? game_instance->GetSubsystem<UGameSubsystem>() : nullptr;
    save_ = IsValid(game_instance) ? game_instance->GetSubsystem<USpaceSaveSubsystem>() : nullptr;
    if (!IsValid(game_)) {
        auto const* const default_theme{GetDefault<USpaceGameUiTheme>()};
        check(IsValid(default_theme));
        fallback_style_ = default_theme->compile();
    }
}

auto UDebugSettingsWidget::RebuildWidget() -> TSharedRef<SWidget> {
    auto const* const style{IsValid(game_) ? &game_->get_ui_style() : &fallback_style_};
    auto const weak_this{TWeakObjectPtr<UDebugSettingsWidget>{this}};
    return SAssignNew(view_, SDebugSettingsView)
        .Style(style)
        .UnlockAllMissions_Lambda([weak_this] {
            auto const* const widget{weak_this.Get()};
            return widget != nullptr && widget->unlock_all_missions();
        })
        .SettingAvailable_Lambda([weak_this] {
            auto const* const widget{weak_this.Get()};
            return widget != nullptr && widget->setting_available();
        })
        .Status_Lambda([weak_this] {
            auto const* const widget{weak_this.Get()};
            return widget != nullptr ? widget->status_text() : FText::GetEmpty();
        })
        .OnUnlockAllMissionsChanged(FOnCheckStateChanged::CreateUObject(
            this, &ThisClass::handle_unlock_all_missions_changed));
}

void UDebugSettingsWidget::ReleaseSlateResources(bool const release_children) {
    Super::ReleaseSlateResources(release_children);
    view_.Reset();
}

auto UDebugSettingsWidget::NativeOnFocusReceived(FGeometry const& geometry,
                                                 FFocusEvent const& focus_event) -> FReply {
    static_cast<void>(geometry);
    static_cast<void>(focus_event);
    focus_content();
    return FReply::Handled();
}

void UDebugSettingsWidget::refresh() {
    save_failed_ = false;
    if (view_.IsValid()) {
        view_->refresh();
    }
}

void UDebugSettingsWidget::focus_content() {
    if (view_.IsValid()) {
        view_->focus_content();
    }
}

auto UDebugSettingsWidget::setting_available() const -> bool {
    return IsValid(save_) && save_->has_active_profile();
}

auto UDebugSettingsWidget::unlock_all_missions() const -> bool {
    return IsValid(save_) && save_->unlock_all_missions();
}

auto UDebugSettingsWidget::status_text() const -> FText {
    if (!setting_available()) {
        return NSLOCTEXT("DebugSettings", "Unavailable", "PROFILE SAVES ARE UNAVAILABLE");
    }
    if (save_failed_) {
        return NSLOCTEXT(
            "DebugSettings", "SaveFailed", "ERROR // DEBUG SETTING COULD NOT BE SAVED");
    }
    return NSLOCTEXT(
        "DebugSettings", "SaveBehavior", "CHANGES SAVE IMMEDIATELY TO THE ACTIVE SERVICE RECORD");
}

void UDebugSettingsWidget::handle_unlock_all_missions_changed(ECheckBoxState const state) {
    auto const enabled{state == ECheckBoxState::Checked};
    if (!IsValid(save_) || !save_->set_unlock_all_missions(enabled)) {
        save_failed_ = true;
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("UDebugSettingsWidget: Failed to save Unlock All Missions."));
    } else {
        save_failed_ = false;
        settings_changed.Broadcast();
    }
    if (view_.IsValid()) {
        view_->refresh();
    }
}
} // namespace ml::ioj
