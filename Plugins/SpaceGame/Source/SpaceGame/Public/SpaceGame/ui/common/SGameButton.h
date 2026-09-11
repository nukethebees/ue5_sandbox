#pragma once

#include "SpaceGamePresentation/audio/GameAudio.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"

#include <Widgets/SCompoundWidget.h>

class SButton;

namespace ml::ioj {

class SPACEGAME_API SGameButton final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SGameButton)
        : _Style(nullptr)
        , _Icon(nullptr)
        , _IconTint(FLinearColor::White)
        , _IconSize(24.0f, 24.0f)
        , _IconSpacing(12.0f)
        , _ContentAlignment(HAlign_Center)
        , _Text()
        , _ToolTipText()
        , _Enabled(true)
        , _Selected(false)
        , _Audio() {}
    SLATE_ARGUMENT(FGameButtonPresentationStyle const*, Style)
    SLATE_ARGUMENT(FSlateBrush const*, Icon)
    SLATE_ATTRIBUTE(FSlateColor, IconTint)
    SLATE_ARGUMENT(FVector2D, IconSize)
    SLATE_ARGUMENT(float, IconSpacing)
    SLATE_ARGUMENT(EHorizontalAlignment, ContentAlignment)
    SLATE_ATTRIBUTE(FText, Text)
    SLATE_ATTRIBUTE(FText, ToolTipText)
    SLATE_ATTRIBUTE(bool, Enabled)
    SLATE_ARGUMENT(bool, Selected)
    SLATE_ARGUMENT(FGameAudioFacade, Audio)
    SLATE_EVENT(FOnClicked, OnClicked)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
    void set_selected(bool selected);
    void focus();
    auto has_focus() const -> bool;
  private:
    auto text_colour() const -> FSlateColor;
    auto focus_brush() const -> FSlateBrush const*;
    void play_pressed_audio();

    FGameButtonPresentationStyle const* style_{};
    FGameAudioFacade audio_{};
    TSharedPtr<SButton> button_{};
    bool selected_{};
};

} // namespace ml::ioj
