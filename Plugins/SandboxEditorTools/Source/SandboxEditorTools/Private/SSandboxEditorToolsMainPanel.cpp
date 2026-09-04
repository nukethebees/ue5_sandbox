#include "SSandboxEditorToolsMainPanel.h"

#include "AlignAxesCheckboxStates.h"
#include "Bool3.h"
#include "GridLayoutShape.h"
#include "LayoutOffsetMode.h"
#include "LayoutSettings.h"
#include "SandboxEditorToolsLogCategories.h"
#include "SandboxEditorToolsSubsystem.h"
#include "SandboxUI/slate/SlateSlots.h"
#include "SandboxUI/widgets/SSectionPanel.h"

#include "Engine/Engine.h"
#include "Layout/Margin.h"
#include "Selection.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SandboxEditorToolsMainPanel"

void SSandboxEditorToolsMainPanel::Construct(FArguments const&) {
    construct_children();
    update_cursor_name();
}
void SSandboxEditorToolsMainPanel::construct_children() {
    FMargin const section_padding{0.0f, 4.0f};
    auto const section_border{FAppStyle::Get().GetBrush("WhiteBrush")};
    FLinearColor const section_background{0.1f, 0.1f, 0.1f};
    auto const make_centered_text{[](FText const& text) -> TSharedRef<SWidget> {
        // clang-format off
        return SNew(STextBlock)
        .Text(text)
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
        +SandboxUI::Slate::vbox_auto_slot(section_padding).VAlign(VAlign_Center)
        [
            SNew(SSectionPanel)
            .Title(LOCTEXT("CursorHeading", "Cursor"))
            .TitleJustification(ETextJustify::Center)
            .TitlePadding(section_padding)
            .BorderImage(section_border)
            .BorderBackgroundColor(section_background)
            .Padding(8.0f)
            [
                SNew(SVerticalBox)
                +SandboxUI::Slate::vbox_auto_slot(section_padding).VAlign(VAlign_Center)
                [
                    SNew(SHorizontalBox)
                    +SandboxUI::Slate::hbox_fill_slot()
                    .VAlign(VAlign_Fill)
                    .HAlign(HAlign_Fill)
                    [
                        SAssignNew(cursor_name, STextBlock)
                        .Text(LOCTEXT("NoCursor", "Cursor name: (no cursor)"))
                        .Justification(ETextJustify::Left)
                    ]
                ]
                +SandboxUI::Slate::vbox_auto_slot(section_padding).VAlign(VAlign_Center)
                [
                    SNew(SHorizontalBox)
                    +SandboxUI::Slate::hbox_fill_slot().HAlign(HAlign_Center)
                    [
                         SNew(SButton)
                        .OnClicked(this, &ThisClass::on_spawn_cursor_button_clicked)
                        .Text(LOCTEXT("Spawn", "Spawn"))
                    ]
                    +SandboxUI::Slate::hbox_fill_slot().HAlign(HAlign_Center)
                    [
                        SNew(SButton)
                        .OnClicked(this, &ThisClass::on_destroy_cursor_button_clicked)
                        .Text(LOCTEXT("Destroy", "Destroy"))
                    ]
                    +SandboxUI::Slate::hbox_fill_slot().HAlign(HAlign_Center)
                    [
                        SNew(SButton)
                        .OnClicked(this, &ThisClass::on_move_cursor_to_button_clicked)
                        .Text(LOCTEXT("MoveToActor", "Move to actor"))
                    ]
                ]
            ]
        ]
        //------------------------------------------------------------------------------------------
        // Alignment
        //------------------------------------------------------------------------------------------
        +SandboxUI::Slate::vbox_auto_slot(section_padding).VAlign(VAlign_Center)
        [
            SNew(SSectionPanel)
            .Title(LOCTEXT("AlignmentHeading", "Alignment"))
            .TitleJustification(ETextJustify::Center)
            .TitlePadding(section_padding)
            .BorderImage(section_border)
            .BorderBackgroundColor(section_background)
            .Padding(8.0f)
            [
                SNew(SVerticalBox)
                +SandboxUI::Slate::vbox_auto_slot(section_padding).VAlign(VAlign_Center)
                [
                    SNew(SButton)
                    .OnClicked(this, &ThisClass::on_look_at_cursor_button_clicked)
                    .Text(LOCTEXT("LookAtCursor", "Look at cursor"))
                ]
                +SandboxUI::Slate::vbox_auto_slot(section_padding).VAlign(VAlign_Center)
                [
                    SNew(SButton)
                    .OnClicked(this, &ThisClass::on_look_away_from_cursor_button_clicked)
                    .Text(LOCTEXT("LookAwayFromCursor", "Look away from cursor"))
                ]
                +SandboxUI::Slate::vbox_auto_slot(section_padding).VAlign(VAlign_Center)
                [
                    SNew(SHorizontalBox)
                    +SandboxUI::Slate::hbox_fill_slot().HAlign(HAlign_Center)
                    [
                         SNew(SCheckBox)
                        .IsChecked(this, &ThisClass::get_align_roll_state)
                        .OnCheckStateChanged(this, &ThisClass::set_align_roll_state)
                        [
                            make_centered_text(LOCTEXT("Roll", "Roll"))
                        ]
                    ]
                    +SandboxUI::Slate::hbox_fill_slot().HAlign(HAlign_Center)
                    [
                        SNew(SCheckBox)
                        .IsChecked(this, &ThisClass::get_align_pitch_state)
                        .OnCheckStateChanged(this, &ThisClass::set_align_pitch_state)
                        [
                            make_centered_text(LOCTEXT("Pitch", "Pitch"))
                        ]
                    ]
                    +SandboxUI::Slate::hbox_fill_slot().HAlign(HAlign_Center)
                    [
                        SNew(SCheckBox)
                        .IsChecked(this, &ThisClass::get_align_yaw_state)
                        .OnCheckStateChanged(this, &ThisClass::set_align_yaw_state)
                        [
                            make_centered_text(LOCTEXT("Yaw", "Yaw"))
                        ]
                    ]
                ]
            ]
        ]
        //------------------------------------------------------------------------------------------
        // Layout
        //------------------------------------------------------------------------------------------
        +SandboxUI::Slate::vbox_auto_slot(section_padding).VAlign(VAlign_Center)
        [
            SNew(SSectionPanel)
            .Title(LOCTEXT("LayoutHeading", "Layout"))
            .TitleJustification(ETextJustify::Center)
            .TitlePadding(section_padding)
            .BorderImage(section_border)
            .BorderBackgroundColor(section_background)
            .Padding(8.0f)
            [
                SNew(SVerticalBox)
                +SandboxUI::Slate::vbox_auto_slot(section_padding).VAlign(VAlign_Center)
                [
                    SNew(SNumericVectorInputBox<double>)
                    .X_Lambda([&]() { return layout_offset.X; })
			        .Y_Lambda([&]() { return layout_offset.Y; })
			        .Z_Lambda([&]() { return layout_offset.Z; })
                    .OnXChanged_Lambda([&](double v){ layout_offset.X = v; })
                    .OnYChanged_Lambda([&](double v){ layout_offset.Y = v; })
                    .OnZChanged_Lambda([&](double v){ layout_offset.Z = v; })
                ]
                +SandboxUI::Slate::vbox_auto_slot(section_padding).VAlign(VAlign_Center)
                [
                    SNew(SButton)
                    .OnClicked(this, &ThisClass::on_align_cube_button_clicked)
                    .Text(LOCTEXT("AlignCube", "Align (Cube)"))
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
        update_cursor_name();
    } else {
        UE_LOG(LogSandboxEditorTools, Error, TEXT("Tools subsystem is nullptr."));
    }

    return FReply::Handled();
}
auto SSandboxEditorToolsMainPanel::on_destroy_cursor_button_clicked() -> FReply {
    auto* ss{get_subsystem()};

    if (ss) {
        ss->destroy_cursor();
        set_cursor_name(TEXT("<none>"));
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
auto SSandboxEditorToolsMainPanel::on_look_away_from_cursor_button_clicked() -> FReply {
    auto* ss{get_subsystem()};
    if (ss) {
        ss->align_actors_away_from_cursor(align_axes_checkbox_states.to_bools());
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

void SSandboxEditorToolsMainPanel::set_cursor_name(FString const& name) {
    if (!cursor_name) {
        UE_LOG(LogSandboxEditorTools, Error, TEXT("cursor_name is nullptr. Cannot set name."));
        return;
    }
    cursor_name->SetText(
        FText::Format(LOCTEXT("CursorNameFormat", "Cursor name: {0}"), FText::FromString(name)));
}
void SSandboxEditorToolsMainPanel::update_cursor_name() {
    auto* ss{get_subsystem()};
    if (!ss) {
        set_cursor_name(TEXT("<unknown>"));
        return;
    }

    set_cursor_name(ss->get_cursor_name());
}

#undef LOCTEXT_NAMESPACE
