#include "SSandboxEditorToolsMainPanel.h"

#include "AlignAxesCheckboxStates.h"
#include "Bool3.h"
#include "GridLayoutShape.h"
#include "LayoutOffsetMode.h"
#include "LayoutSettings.h"
#include "SandboxEditorToolsLogCategories.h"
#include "SandboxEditorToolsSubsystem.h"

#include "Engine/Engine.h"
#include "Selection.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SSandboxEditorToolsMainPanel::Construct(FArguments const& in_args) {
    // clang-format off
    ChildSlot
    [
        //------------------------------------------------------------------------------------------
        // Cursor
        //------------------------------------------------------------------------------------------
        SNew(SVerticalBox)
        +SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Fill)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Cursor controls")))
            .Justification(ETextJustify::Center)
        ]
        +SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Fill)
        [
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot() 
            .FillWidth(1.f)
            .VAlign(VAlign_Fill)
            [
                 SNew(SButton)
                .OnClicked(this, &ThisClass::on_spawn_cursor_button_clicked)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Spawn")))
                    .Justification(ETextJustify::Center)
                ]
            ]
            +SHorizontalBox::Slot() 
            .FillWidth(1.f)
            .VAlign(VAlign_Fill)
            [
                SNew(SButton)
                .OnClicked(this, &ThisClass::on_destroy_cursor_button_clicked)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Destroy")))
                    .Justification(ETextJustify::Center)
                ]
            ]
            +SHorizontalBox::Slot() 
            .FillWidth(1.f)
            .VAlign(VAlign_Fill)
            [
                SNew(SButton)
                .OnClicked(this, &ThisClass::on_move_cursor_to_button_clicked)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Move to actor")))
                    .Justification(ETextJustify::Center)
                ]
            ]
        ]
        //------------------------------------------------------------------------------------------
        // Alignment
        //------------------------------------------------------------------------------------------
        +SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Fill)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Alignment")))
            .Justification(ETextJustify::Center)
        ]
        +SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Fill)
        [
            SNew(SButton)
            .OnClicked(this, &ThisClass::on_look_at_cursor_button_clicked)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Look at cursor")))
                .Justification(ETextJustify::Center)
            ]
        ]
        +SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Fill)
        [
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot() 
            .FillWidth(1.f)
            .VAlign(VAlign_Fill)
            .HAlign(EHorizontalAlignment::HAlign_Center)
            [
                 SNew(SCheckBox)
                .IsChecked(this, &ThisClass::get_align_roll_state)
                .OnCheckStateChanged(this, &ThisClass::set_align_roll_state)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Roll")))
                    .Justification(ETextJustify::Center)
                ]
            ]
            +SHorizontalBox::Slot() 
            .FillWidth(1.f)
            .VAlign(VAlign_Fill)
            .HAlign(EHorizontalAlignment::HAlign_Center)
            [
                SNew(SCheckBox)
                .IsChecked(this, &ThisClass::get_align_pitch_state)
                .OnCheckStateChanged(this, &ThisClass::set_align_pitch_state)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Pitch")))
                    .Justification(ETextJustify::Center)
                ]
            ]
            +SHorizontalBox::Slot() 
            .FillWidth(1.f)
            .VAlign(VAlign_Fill)
            .HAlign(EHorizontalAlignment::HAlign_Center)
            [
                SNew(SCheckBox)
                .IsChecked(this, &ThisClass::get_align_yaw_state)
                .OnCheckStateChanged(this, &ThisClass::set_align_yaw_state)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Yaw")))
                    .Justification(ETextJustify::Center)
                ]
            ]
        ]
        //------------------------------------------------------------------------------------------
        // Layout
        //------------------------------------------------------------------------------------------
        +SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Fill)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Layout")))
            .Justification(ETextJustify::Center)
        ]
        +SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Fill)
        [
            SNew(SNumericVectorInputBox<double>)
            .X_Lambda([&]() { return layout_offset.X; })
			.Y_Lambda([&]() { return layout_offset.Y; })
			.Z_Lambda([&]() { return layout_offset.Z; })
            .OnXChanged_Lambda([&](double v){ layout_offset.X = v; })
            .OnYChanged_Lambda([&](double v){ layout_offset.Y = v; })
            .OnZChanged_Lambda([&](double v){ layout_offset.Z = v; })
        ]
        +SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Fill)
        [
            SNew(SButton)
            .OnClicked(this, &ThisClass::on_align_cube_button_clicked)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Align cube")))
                .Justification(ETextJustify::Center)
            ]
        ]
    ];
    // clang-format on
}
auto SSandboxEditorToolsMainPanel::on_move_cursor_to_button_clicked() -> FReply {
    auto* ss{get_subsystem()};
    auto* selected_actors{GEditor->GetSelectedActors()};

    if (selected_actors && selected_actors->Num() > 0) {
        auto* actor{Cast<AActor>(selected_actors->GetSelectedObject(0))};
        ss->move_cursor_to_actor(actor);
    } else {
        UE_LOG(LogSandboxEditorTools, Display, TEXT("No actors to select"));
    }

    return FReply::Handled();
}
auto SSandboxEditorToolsMainPanel::on_spawn_cursor_button_clicked() -> FReply {
    auto* ss{get_subsystem()};

    if (ss) {
        ss->get_cursor();
    } else {
        UE_LOG(LogSandboxEditorTools, Error, TEXT("Tools subsystem is nullptr."));
    }

    return FReply::Handled();
}
auto SSandboxEditorToolsMainPanel::on_destroy_cursor_button_clicked() -> FReply {
    auto* ss{get_subsystem()};

    if (ss) {
        ss->destroy_cursor();
    } else {
        UE_LOG(LogSandboxEditorTools, Error, TEXT("Tools subsystem is nullptr."));
    }

    return FReply::Handled();
}
auto SSandboxEditorToolsMainPanel::on_look_at_cursor_button_clicked() -> FReply {
    auto* ss{get_subsystem()};
    if (ss) {
        ss->align_actors_to_cursor(align_axes_checkbox_states.to_bools());
    }

    return FReply::Handled();
}
auto SSandboxEditorToolsMainPanel::on_align_cube_button_clicked() -> FReply {
    auto* ss{get_subsystem()};
    if (ss) {
        ss->position_actors(FLayoutSettings{
            .shape = EGridLayoutShape::Cuboid,
            .offset_mode = ELayoutOffsetMode::CentreToCentre,
            .offset = layout_offset,
        });
    }

    return FReply::Handled();
}

auto SSandboxEditorToolsMainPanel::get_align_roll_state() const -> ECheckBoxState {
    return align_axes_checkbox_states.roll;
}
void SSandboxEditorToolsMainPanel::set_align_roll_state(ECheckBoxState state) {
    align_axes_checkbox_states.roll = state;
}
auto SSandboxEditorToolsMainPanel::get_align_pitch_state() const -> ECheckBoxState {
    return align_axes_checkbox_states.pitch;
}
void SSandboxEditorToolsMainPanel::set_align_pitch_state(ECheckBoxState state) {
    align_axes_checkbox_states.pitch = state;
}
auto SSandboxEditorToolsMainPanel::get_align_yaw_state() const -> ECheckBoxState {
    return align_axes_checkbox_states.yaw;
}
void SSandboxEditorToolsMainPanel::set_align_yaw_state(ECheckBoxState state) {
    align_axes_checkbox_states.yaw = state;
}

auto SSandboxEditorToolsMainPanel::get_subsystem() -> USandboxEditorToolsSubsystem* {
    check(GEditor);
    auto* ss{GEditor->GetEditorSubsystem<USandboxEditorToolsSubsystem>()};
    return ss;
}
