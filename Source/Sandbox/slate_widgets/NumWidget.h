// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <optional>
#include <type_traits>
#include <utility>

#include "Components/Widget.h"
#include "CoreMinimal.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Widgets/SCompoundWidget.h"

#include "NumWidget.generated.h"

template <typename T>
class SNumWidget
    : public SCompoundWidget
    , public ml::LogMsgMixin<"SNumWidget"> {
  public:
    // clang-format off
    SLATE_BEGIN_ARGS(SNumWidget) {}
        SLATE_ARGUMENT(FText, label)
        SLATE_ARGUMENT(T, value)
        SLATE_ARGUMENT(std::optional<T>, max_value)
    SLATE_END_ARGS()
    // clang-format on

    void Construct(FArguments const& InArgs);

    // Getters
    auto const& get_label() const { return label_; }
    auto const& get_value() const { return value_; }
    auto const& get_max_value() const { return max_value_; }

    // Setters
    void set_label(FText const& new_label);
    void set_value(T const& new_value);
    void set_max_value(std::optional<T> const& new_max_value);

    // Data display
    FText get_display_text() const;
  private:
    FText label_;
    T value_;
    std::optional<T> max_value_;

    // Caching members for expensive text formatting
    mutable FText cached_display_text_;
    mutable bool is_display_text_dirty_{true};
};

extern template class SNumWidget<float>;
extern template class SNumWidget<int32>;

#define CREATE_SETTER(NAME)                                    \
    template <typename Self, typename T>                       \
    void set_##NAME(this Self& self, T&& x) {                  \
        if (self.slate_widget) {                               \
            self.slate_widget->set_##NAME(std::forward<T>(x)); \
        } else {                                               \
            self.log_warning(TEXT("Widget is nullptr."));      \
        }                                                      \
    }
class UNumWidgetMixins {
  public:
    CREATE_SETTER(label)
    CREATE_SETTER(value)
    CREATE_SETTER(max_value)

    TSharedRef<SWidget> rebuild_widget(this auto& self) {
        using T = typename std::remove_cvref_t<decltype(self)>::NumT;

        self.slate_widget = SNew(SNumWidget<T>).label(FText::FromString(TEXT("Default")));
        return self.slate_widget.ToSharedRef();
    }
};
#undef CREATE_SETTER

UCLASS()
class UFloatNumWidget
    : public UWidget
    , public UNumWidgetMixins
    , public ml::LogMsgMixin<"UFloatNumWidget"> {
    GENERATED_BODY()
    friend class UNumWidgetMixins;
  public:
    using NumT = float;
  protected:
    virtual TSharedRef<SWidget> RebuildWidget() override { return rebuild_widget(); }
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
  private:
    TSharedPtr<SNumWidget<NumT>> slate_widget;
};

UCLASS()
class UIntNumWidget
    : public UWidget
    , public UNumWidgetMixins
    , public ml::LogMsgMixin<"UIntNumWidget"> {
    GENERATED_BODY()
    friend class UNumWidgetMixins;
  public:
    using NumT = int32;
  protected:
    virtual TSharedRef<SWidget> RebuildWidget() override { return rebuild_widget(); }
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
  private:
    TSharedPtr<SNumWidget<NumT>> slate_widget;
};
