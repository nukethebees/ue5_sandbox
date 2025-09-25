#include "Sandbox/slate_widgets/NumWidget.h"

#include "Fonts/CompositeFont.h"
#include "Sandbox/utilities/ui.h"
#include "SlateOptMacros.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
template <typename T>
void SNumWidget<T>::Construct(FArguments const& InArgs) {
    label_ = InArgs._label;
    value_ = InArgs._value;
    max_value_ = InArgs._max_value;

    auto const font_size{ml::scale_font_to_umg(32)};
    auto const style{FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Bold", font_size))};

    // clang-format off
    ChildSlot[
        SNew(STextBlock)
            .Text(this, &SNumWidget::get_display_text)
            .Font(style)
            .ColorAndOpacity(FSlateColor(FLinearColor::White))
    ];
    // clang-format on
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

template <typename T>
void SNumWidget<T>::set_label(FText const& new_label) {
    if (!label_.EqualTo(new_label)) {
        label_ = new_label;
        is_display_text_dirty_ = true;
    }
}
template <typename T>
void SNumWidget<T>::set_value(T const& new_value) {
    log_verbose(TEXT("set_value"));
    if (value_ != new_value) {
        value_ = new_value;
        is_display_text_dirty_ = true;
    }
}
template <typename T>
void SNumWidget<T>::set_max_value(std::optional<T> const& new_max_value) {
    if (max_value_ != new_max_value) {
        max_value_ = new_max_value;
        is_display_text_dirty_ = true;
    }
}

template <typename T>
FText SNumWidget<T>::get_display_text() const {
    if (is_display_text_dirty_) {
        if (max_value_) {
            cached_display_text_ =
                FText::Format(NSLOCTEXT("StatWidget", "ValueWithMax", "{0}: {1} / {2}"),
                              label_,
                              FText::AsNumber(value_),
                              FText::AsNumber(*max_value_));
        } else {
            cached_display_text_ = FText::Format(
                NSLOCTEXT("StatWidget", "ValueOnly", "{0}: {1}"), label_, FText::AsNumber(value_));
        }
        is_display_text_dirty_ = false;
    }
    return cached_display_text_;
}

TSharedRef<SWidget> UFloatNumWidget::RebuildWidget() {
    slate_widget = SNew(SNumWidget<float>).label(FText::FromString(TEXT("Default"))).value(0);

    return slate_widget.ToSharedRef();
}
void UFloatNumWidget::ReleaseSlateResources(bool bReleaseChildren) {
    Super::ReleaseSlateResources(bReleaseChildren);
    slate_widget.Reset();
}

TSharedRef<SWidget> UIntNumWidget::RebuildWidget() {
    slate_widget = SNew(SNumWidget<int32>).label(FText::FromString(TEXT("Default"))).value(0);

    return slate_widget.ToSharedRef();
}
void UIntNumWidget::ReleaseSlateResources(bool bReleaseChildren) {
    Super::ReleaseSlateResources(bReleaseChildren);
    slate_widget.Reset();
}

template class SNumWidget<float>;
template class SNumWidget<int32>;
