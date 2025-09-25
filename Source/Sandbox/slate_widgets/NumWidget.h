// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <optional>
#include <utility>

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Components/Widget.h"
#include "SlateOptMacros.h"

#include "NumWidget.generated.h"

template <typename T>
class SANDBOX_API SNumWidget : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SNumWidget) {}
    SLATE_ARGUMENT(FText, label)
    SLATE_ARGUMENT(T, value)
    SLATE_ARGUMENT(std::optional<T>, max_value)
    SLATE_END_ARGS()

    BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
    void Construct(FArguments const& InArgs) {
        label_ = InArgs._label;
        value_ = InArgs._value;
        max_value_ = InArgs._max_value;

        ChildSlot[SNew(STextBlock).Text(this, &SNumWidget::get_display_text)];
    }
    END_SLATE_FUNCTION_BUILD_OPTIMIZATION

    FText get_display_text() const {
        if (is_display_text_dirty_) {
            if (max_value_) {
                cached_display_text_ =
                    FText::Format(NSLOCTEXT("StatWidget", "ValueWithMax", "{0}: {1} / {2}"),
                                  label_,
                                  FText::AsNumber(value_),
                                  FText::AsNumber(*max_value_));
            } else {
                cached_display_text_ =
                    FText::Format(NSLOCTEXT("StatWidget", "ValueOnly", "{0}: {1}"),
                                  label_,
                                  FText::AsNumber(value_));
            }
            is_display_text_dirty_ = false;
        }
        return cached_display_text_;
    }

    // Getters
    auto const& get_label() const { return label_; }
    auto const& get_value() const { return value_; }
    auto const& get_max_value() const { return max_value_; }

    // Setters
    void set_label(FText const& new_label) {
        if (!label_.EqualTo(new_label)) {
            label_ = new_label;
            is_display_text_dirty_ = true;
        }
    }

    void set_value(T const& new_value) {
        if (value_ != new_value) {
            value_ = new_value;
            is_display_text_dirty_ = true;
        }
    }

    void set_max_value(std::optional<T> const& new_max_value) {
        if (max_value_ != new_max_value) {
            max_value_ = new_max_value;
            is_display_text_dirty_ = true;
        }
    }
  private:
    FText label_;
    T value_;
    std::optional<T> max_value_;

    // Caching members for expensive text formatting
    mutable FText cached_display_text_;
    mutable bool is_display_text_dirty_{true};
};

UCLASS()
class UFloatNumWidget : public UWidget {
    GENERATED_BODY()
  public:
  protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
  private:
    TSharedPtr<SNumWidget<float>> slate_widget;
};

UCLASS()
class UIntNumWidget : public UWidget {
    GENERATED_BODY()
  public:
  protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
  private:
    TSharedPtr<SNumWidget<int32>> slate_widget;
};
