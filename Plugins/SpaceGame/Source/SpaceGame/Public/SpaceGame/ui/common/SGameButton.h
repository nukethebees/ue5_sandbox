#pragma once

#include "SpaceGame/ui/style/GameUiStyle.h"

#include <Widgets/SCompoundWidget.h>

class SButton;

namespace ml::ioj {

class SPACEGAME_API SGameButton final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SGameButton)
        : _Style(nullptr)
        , _Text()
        , _ToolTipText()
        , _Enabled(true)
        , _Selected(false) {}
    SLATE_ARGUMENT(FGameButtonPresentationStyle const*, Style)
    SLATE_ATTRIBUTE(FText, Text)
    SLATE_ATTRIBUTE(FText, ToolTipText)
    SLATE_ATTRIBUTE(bool, Enabled)
    SLATE_ARGUMENT(bool, Selected)
    SLATE_EVENT(FOnClicked, OnClicked)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
    void set_selected(bool selected);
    void focus();
    auto has_focus() const -> bool;
  private:
    auto text_colour() const -> FSlateColor;

    FGameButtonPresentationStyle const* style_{};
    TSharedPtr<SButton> button_{};
    bool selected_{};
};

} // namespace ml::ioj
