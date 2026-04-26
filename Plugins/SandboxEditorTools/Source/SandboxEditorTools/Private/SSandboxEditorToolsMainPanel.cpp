#include "SSandboxEditorToolsMainPanel.h"

#include "AlignAxesCheckboxStates.h"
#include "Bool3.h"
#include "GridLayoutShape.h"
#include "LayoutOffsetMode.h"
#include "LayoutSettings.h"
#include "SandboxEditorToolsLogCategories.h"
#include "SandboxEditorToolsSubsystem.h"

#include "Engine/Engine.h"
#include "Layout/Margin.h"
#include "Selection.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SSandboxEditorToolsSection::Construct(FArguments const& in_args) {
    auto const border_image{FAppStyle::Get().GetBrush("WhiteBrush")};
    auto const border_bg_colour{FLinearColor(0.1f, 0.1f, 0.1f)};
    auto const border_padding{8.f};

    // clang-format off
    ChildSlot
    [
        SNew(SBorder)
        .Padding(border_padding)
        .BorderImage(border_image)
        .BorderBackgroundColor(border_bg_colour)
        .Content()
        [
            in_args._Content.Widget
        ]
    ];
    // clang-format on
}

void SSandboxEditorToolsMainPanel::Construct(FArguments const& in_args) {
    FMargin const heading_padding{0.f, 4.f};

    constexpr auto add_vlist_item{[]() -> SVerticalBox::FSlot::FSlotArguments {
        auto const p{4.f};
        FMargin const section_padding{0.f, p, 0.f, p};

        // clang-format off
        return MoveTemp(SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Fill)
        .VAlign(EVerticalAlignment::VAlign_Center)
        .Padding(section_padding));
        // clang-format on
    }};

    auto add_hlist_item{[&]() -> SHorizontalBox::FSlot::FSlotArguments {
        // clang-format off
        return MoveTemp(SHorizontalBox::Slot() 
        .FillWidth(1.f)
        .VAlign(VAlign_Fill)
        .HAlign(EHorizontalAlignment::HAlign_Center));
        // clang-format on
    }};

    auto add_heading{[&](TCHAR const* text) -> auto {
        // clang-format off
        return SNew(STextBlock)
        .Text(FText::FromString(text))
        .Justification(ETextJustify::Center);
        // clang-format on
    }};

    auto add_button_text{[&](TCHAR const* text) -> auto {
        // clang-format off
        return SNew(STextBlock)
        .Text(FText::FromString(text))
        .Justification(ETextJustify::Center);
        // clang-format on
    }};

    // clang-format off
    ChildSlot
    [
        //------------------------------------------------------------------------------------------
        // Cursor
        //------------------------------------------------------------------------------------------
        SNew(SVerticalBox)
        +add_vlist_item()
        [
            SNew(SSandboxEditorToolsSection)
            [
                SNew(SVerticalBox)
                +add_vlist_item()
                [
                    add_heading(TEXT("Cursor controls"))
                ]
                +add_vlist_item()
                [
                    SNew(SHorizontalBox)
                    +add_hlist_item()
                    [
                         SNew(SButton)
                        .OnClicked(this, &ThisClass::on_spawn_cursor_button_clicked)
                        [
                            add_button_text(TEXT("Spawn"))
                        ]
                    ]
                    +add_hlist_item()
                    [
                        SNew(SButton)
                        .OnClicked(this, &ThisClass::on_destroy_cursor_button_clicked)
                        [
                            add_button_text(TEXT("Destroy"))
                        ]
                    ]
                    +add_hlist_item()
                    [
                        SNew(SButton)
                        .OnClicked(this, &ThisClass::on_move_cursor_to_button_clicked)
                        [
                            add_button_text(TEXT("Move to actor"))
                        ]
                    ]
                ]
            ]
        ]
        //------------------------------------------------------------------------------------------
        // Alignment
        //------------------------------------------------------------------------------------------
        +add_vlist_item()
        [
            SNew(SSandboxEditorToolsSection)
            [
                SNew(SVerticalBox)
                +add_vlist_item()
                [
                    add_heading(TEXT("Alignment"))
                ]
                +add_vlist_item()
                [
                    SNew(SButton)
                    .OnClicked(this, &ThisClass::on_look_at_cursor_button_clicked)
                    [
                        add_button_text(TEXT("Look at cursor"))
                    ]
                ]
                +add_vlist_item()
                [
                    SNew(SHorizontalBox)
                    +add_hlist_item()
                    [
                         SNew(SCheckBox)
                        .IsChecked(this, &ThisClass::get_align_roll_state)
                        .OnCheckStateChanged(this, &ThisClass::set_align_roll_state)
                        [
                            add_button_text(TEXT("Roll"))
                        ]
                    ]
                    +add_hlist_item()
                    [
                        SNew(SCheckBox)
                        .IsChecked(this, &ThisClass::get_align_pitch_state)
                        .OnCheckStateChanged(this, &ThisClass::set_align_pitch_state)
                        [
                            add_button_text(TEXT("Pitch"))
                        ]
                    ]
                    +add_hlist_item()
                    [
                        SNew(SCheckBox)
                        .IsChecked(this, &ThisClass::get_align_yaw_state)
                        .OnCheckStateChanged(this, &ThisClass::set_align_yaw_state)
                        [
                            add_button_text(TEXT("Yaw"))
                        ]
                    ]
                ]
            ]
        ]
        //------------------------------------------------------------------------------------------
        // Layout
        //------------------------------------------------------------------------------------------
        +add_vlist_item()
        [
            SNew(SSandboxEditorToolsSection)
            [
                SNew(SVerticalBox)
                +add_vlist_item()
                [
                    add_heading(TEXT("Layout"))
                ]
                +add_vlist_item()
                [
                    SNew(SNumericVectorInputBox<double>)
                    .X_Lambda([&]() { return layout_offset.X; })
			        .Y_Lambda([&]() { return layout_offset.Y; })
			        .Z_Lambda([&]() { return layout_offset.Z; })
                    .OnXChanged_Lambda([&](double v){ layout_offset.X = v; })
                    .OnYChanged_Lambda([&](double v){ layout_offset.Y = v; })
                    .OnZChanged_Lambda([&](double v){ layout_offset.Z = v; })
                ]
                +add_vlist_item()
                [
                    SNew(SButton)
                    .OnClicked(this, &ThisClass::on_align_cube_button_clicked)
                    [
                        add_button_text(TEXT("Align (Cube)"))
                    ]
                ]
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
