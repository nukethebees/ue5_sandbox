#include "SpaceGamePresentation/presentation/widgets/FlightVectorDebugWidget.h"

#include "SpaceGamePresentation/presentation/widgets/Vector2DWidget.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"

#include <Blueprint/WidgetTree.h>
#include <Components/UniformGridPanel.h>
#include <Components/UniformGridSlot.h>

void UFlightVectorDebugWidget::NativePreConstruct() {
    Super::NativePreConstruct();

    if (IsDesignTime()) {
        data_ = {
            .turn_input = FVector2D{0.4f, -0.2f},
            .move_input = FVector2D{-0.3f, 0.8f},
            .target_velocity = FVector2D{0.2f, 0.9f},
            .local_velocity = FVector2D{-0.6f, 0.7f},
        };
    }

    rebuild_widget_tree();
    update_widgets();
}

void UFlightVectorDebugWidget::update(ml::ship_hud::FFlightVectorDebugData const& data) {
    data_ = data;
    update_widgets();
}

void UFlightVectorDebugWidget::apply_hud_style(ml::ioj::FGameHudStyle const& style) {
    hud_style_ = style;
    update_widgets();
}

void UFlightVectorDebugWidget::set_font_size(int32 const new_font_size) {
    font_size = new_font_size;
    update_widgets();
}

void UFlightVectorDebugWidget::rebuild_widget_tree() {
    if (!WidgetTree) {
        return;
    }

    if (!vector_grid_) {
        vector_grid_ = WidgetTree->ConstructWidget<UUniformGridPanel>(
            UUniformGridPanel::StaticClass(), TEXT("flight_vector_grid"));
        WidgetTree->RootWidget = vector_grid_;
    }

    configure_vector_widget(turn_input_widget_, TEXT("turn_input_widget"), INVTEXT("Turning"));
    configure_vector_widget(move_input_widget_, TEXT("move_input_widget"), INVTEXT("Movement"));
    configure_vector_widget(
        target_velocity_widget_, TEXT("target_velocity_widget"), INVTEXT("Target velocity"));
    configure_vector_widget(
        local_velocity_widget_, TEXT("local_velocity_widget"), INVTEXT("Local velocity"));
}

void UFlightVectorDebugWidget::update_widgets() {
    for (auto* const widget : {turn_input_widget_,
                               move_input_widget_,
                               target_velocity_widget_,
                               local_velocity_widget_}) {
        if (!IsValid(widget)) {
            continue;
        }

        widget->set_font_size(font_size);
        if (hud_style_) {
            widget->apply_hud_style(hud_style_.GetValue());
        }
    }

    if (turn_input_widget_) {
        turn_input_widget_->update(data_.turn_input);
    }
    if (move_input_widget_) {
        move_input_widget_->update(data_.move_input);
    }
    if (target_velocity_widget_) {
        target_velocity_widget_->update(data_.target_velocity);
    }
    if (local_velocity_widget_) {
        local_velocity_widget_->update(data_.local_velocity);
    }
}

void UFlightVectorDebugWidget::configure_vector_widget(UVector2DWidget*& widget,
                                                       FName const name,
                                                       FText label) {
    if (!widget) {
        widget = WidgetTree->ConstructWidget<UVector2DWidget>(UVector2DWidget::StaticClass(), name);
    }
    widget->set_label(MoveTemp(label));

    if (widget->GetParent() == vector_grid_) {
        return;
    }

    auto* const slot{vector_grid_->AddChildToUniformGrid(widget)};
    slot->SetHorizontalAlignment(HAlign_Fill);
    slot->SetVerticalAlignment(VAlign_Fill);

    if (widget == turn_input_widget_) {
        slot->SetRow(0);
        slot->SetColumn(0);
    } else if (widget == move_input_widget_) {
        slot->SetRow(0);
        slot->SetColumn(1);
    } else if (widget == target_velocity_widget_) {
        slot->SetRow(1);
        slot->SetColumn(0);
    } else {
        slot->SetRow(1);
        slot->SetColumn(1);
    }
}
