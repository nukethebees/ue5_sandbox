#include "Sandbox/ui/widgets/NumWidget.h"

#include "Fonts/CompositeFont.h"
#include "SlateOptMacros.h"

#include "Sandbox/ui/SandboxStyle.h"
#include "Sandbox/ui/ui.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
template <typename T>
void SNumWidget<T>::Construct(FArguments const& InArgs) {
    label_ = InArgs._label;
    value_ = InArgs._value;
    max_value_ = InArgs._max_value;

    // clang-format off
    ChildSlot[
        SAssignNew(text_block_, STextBlock)
            .TextStyle(&ml::SandboxStyle::get(), "Sandbox.Text.Widget")
    ];
    // clang-format on
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

template <typename T>
void SNumWidget<T>::set_label(FText const& new_label) {
    if (!label_.EqualTo(new_label)) {
        label_ = new_label;
        update_display_text();
    }
}
template <typename T>
void SNumWidget<T>::set_value(T const& new_value) {
    if (value_ != new_value) {
        value_ = new_value;
        update_display_text();
    }
}
template <typename T>
void SNumWidget<T>::set_max_value(std::optional<T> const& new_max_value) {
    if (max_value_ != new_max_value) {
        max_value_ = new_max_value;
        update_display_text();
    }
}

template <typename T>
void SNumWidget<T>::update_display_text() const {
    FText text{};

    if (max_value_) {
        text = FText::Format(NSLOCTEXT("StatWidget", "ValueWithMax", "{0}: {1} / {2}"),
                             label_,
                             FText::AsNumber(value_),
                             FText::AsNumber(*max_value_));
    } else {
        text = FText::Format(
            NSLOCTEXT("StatWidget", "ValueOnly", "{0}: {1}"), label_, FText::AsNumber(value_));
    }

    text_block_->SetText(text);
}

void UFloatNumWidget::ReleaseSlateResources(bool bReleaseChildren) {
    Super::ReleaseSlateResources(bReleaseChildren);
    slate_widget.Reset();
}
void UIntNumWidget::ReleaseSlateResources(bool bReleaseChildren) {
    Super::ReleaseSlateResources(bReleaseChildren);
    slate_widget.Reset();
}

template class SNumWidget<float>;
template class SNumWidget<int32>;
