#pragma once

#include "SpaceGame/ui/main_menu/MainMenuWidget.h"

#include <Widgets/SBoxPanel.h>
#include <Widgets/SCompoundWidget.h>

class SBox;
class SWidgetSwitcher;

namespace ml::ioj {
class SHiveNavigationButton;

DECLARE_DELEGATE_OneParam(FOnMainMenuPageSelected, EMainMenuPage);

class SMainMenuView final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SMainMenuView)
        : _Style(nullptr)
        , _InitialPage(EMainMenuPage::SelectMission) {}
    SLATE_ARGUMENT(FGameUiStyle const*, Style)
    SLATE_ARGUMENT(EMainMenuPage, InitialPage)
    SLATE_NAMED_SLOT(FArguments, MissionContent)
    SLATE_NAMED_SLOT(FArguments, ArchiveContent)
    SLATE_NAMED_SLOT(FArguments, OptionsContent)
    SLATE_NAMED_SLOT(FArguments, DebugContent)
    SLATE_EVENT(FOnMainMenuPageSelected, OnPageSelected)
    SLATE_EVENT(FSimpleDelegate, OnQuit)
    SLATE_EVENT(FSimpleDelegate, OnFocusContent)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
    void set_active_page(EMainMenuPage page);
    void set_mission_content(TSharedRef<SWidget> content);
    void set_navigation_enabled(bool enabled);
    void focus_navigation();
    void focus_content_on_next_focus();

    auto SupportsKeyboardFocus() const -> bool override { return true; }
    auto OnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
    auto OnKeyDown(FGeometry const& geometry, FKeyEvent const& key_event) -> FReply override;
  private:
    auto build_header() const -> TSharedRef<SWidget>;
    auto build_navigation() -> TSharedRef<SWidget>;
    auto build_footer() const -> TSharedRef<SWidget>;
    void add_section(TSharedRef<SVerticalBox> const& navigation, FText text) const;
    void add_page(TSharedRef<SVerticalBox> const& navigation,
                  EMainMenuPage page,
                  FText text,
                  EGameUiIcon icon);
    auto handle_page(EMainMenuPage page) -> FReply;
    auto handle_quit() -> FReply;
    auto page_content_index(EMainMenuPage page) const -> int32;
    auto navigation_has_focus() const -> bool;

    FGameUiStyle const* style_{};
    FOnMainMenuPageSelected on_page_selected_{};
    FSimpleDelegate on_quit_{};
    FSimpleDelegate on_focus_content_{};
    TArray<TSharedPtr<SHiveNavigationButton>> page_buttons_{};
    TSharedPtr<SHiveNavigationButton> quit_button_{};
    TSharedPtr<SWidgetSwitcher> content_switcher_{};
    TSharedPtr<SBox> mission_content_{};
    EMainMenuPage active_page_{EMainMenuPage::SelectMission};
    bool navigation_enabled_{true};
    bool focus_content_next_{};
};
} // namespace ml::ioj
