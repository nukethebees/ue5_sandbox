#pragma once

#include "SpaceGamePresentation/audio/GameAudio.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"
#include <ioj/menu/player_menu.h>

#include <Widgets/SBoxPanel.h>
#include <Widgets/SCompoundWidget.h>

class SBox;
class SWidgetSwitcher;

namespace ioj {
class SHiveNavigationButton;

DECLARE_DELEGATE_OneParam(FOnMainMenuItem, MainMenuItem);

class SMainMenuView final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SMainMenuView)
        : _Style(nullptr)
        , _Page(MainMenuItem::SelectLevel)
        , _Audio() {}
    SLATE_ARGUMENT(FGameUiStyle const*, Style)
    SLATE_ATTRIBUTE(MainMenuItem, Page)
    SLATE_ARGUMENT(FGameAudioFacade, Audio)
    SLATE_NAMED_SLOT(FArguments, LevelContent)
    SLATE_NAMED_SLOT(FArguments, ArchiveContent)
    SLATE_NAMED_SLOT(FArguments, OptionsContent)
    SLATE_EVENT(FOnMainMenuItem, OnItem)
    SLATE_EVENT(FSimpleDelegate, OnRestoreFocus)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
    void refresh_page();
    void set_level_content(TSharedRef<SWidget> content);
    void set_navigation_enabled(bool enabled);
    void focus_item(MainMenuItem item);

    auto SupportsKeyboardFocus() const -> bool override { return true; }
    auto OnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
  private:
    auto build_header() const -> TSharedRef<SWidget>;
    auto build_navigation() -> TSharedRef<SWidget>;
    auto build_footer() const -> TSharedRef<SWidget>;
    void add_section(TSharedRef<SVerticalBox> const& navigation, FText text) const;
    void add_page(TSharedRef<SVerticalBox> const& navigation,
                  MainMenuItem page,
                  FText text,
                  EGameUiIcon icon);
    auto activate_item(MainMenuItem item) -> FReply;
    auto page_content_index(MainMenuItem page) const -> int32;

    FGameUiStyle const* style_{};
    FGameAudioFacade audio_{};
    FOnMainMenuItem on_item_{};
    FSimpleDelegate on_restore_focus_{};
    TArray<TSharedPtr<SHiveNavigationButton>> page_buttons_{};
    TSharedPtr<SHiveNavigationButton> quit_button_{};
    TSharedPtr<SWidgetSwitcher> content_switcher_{};
    TSharedPtr<SBox> level_content_{};
    TAttribute<MainMenuItem> page_;
    bool navigation_enabled_{true};
};
} // namespace ioj
