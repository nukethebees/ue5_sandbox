#pragma once

#include "SpaceGame/ui/style/GameUiStyle.h"

#include <Widgets/SCompoundWidget.h>

namespace ml::ioj {
class SGameButton;

class SPACEGAME_API SHiveFrame final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SHiveFrame)
        : _Style(nullptr) {}
    SLATE_ARGUMENT(FGameUiChromeStyle const*, Style)
    SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
};

class SPACEGAME_API SHiveSectionHeader final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SHiveSectionHeader)
        : _Style(nullptr)
        , _Icon(nullptr)
        , _Text() {}
    SLATE_ARGUMENT(FGameUiStyle const*, Style)
    SLATE_ARGUMENT(FSlateBrush const*, Icon)
    SLATE_ATTRIBUTE(FText, Text)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
};

class SPACEGAME_API SHiveNavigationButton final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SHiveNavigationButton)
        : _Style(nullptr)
        , _Icon(nullptr)
        , _Text()
        , _Enabled(true)
        , _Selected(false) {}
    SLATE_ARGUMENT(FGameUiStyle const*, Style)
    SLATE_ARGUMENT(FSlateBrush const*, Icon)
    SLATE_ATTRIBUTE(FText, Text)
    SLATE_ATTRIBUTE(bool, Enabled)
    SLATE_ARGUMENT(bool, Selected)
    SLATE_EVENT(FOnClicked, OnClicked)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
    void set_selected(bool selected);
    void focus();
    auto has_focus() const -> bool;
  private:
    auto icon_colour() const -> FSlateColor;

    FGameUiStyle const* style_{};
    TSharedPtr<SGameButton> button_{};
    bool selected_{};
};
}
