#pragma once

#include "AlignAxesCheckboxStates.h"

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"

class STextBlock;
class SEditableTextBox;

class USandboxEditorToolsSubsystem;

class SSandboxEditorToolsMainPanel : public SCompoundWidget {
  public:
    using ThisClass = SSandboxEditorToolsMainPanel;

    SLATE_BEGIN_ARGS(class SSandboxEditorToolsMainPanel) {}
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
  protected:
    auto get_subsystem() -> USandboxEditorToolsSubsystem*;

    auto on_move_cursor_to_button_clicked() -> FReply;
    auto on_spawn_cursor_button_clicked() -> FReply;
    auto on_destroy_cursor_button_clicked() -> FReply;
    auto on_look_at_cursor_button_clicked() -> FReply;
    auto on_align_cube_button_clicked() -> FReply;

    auto get_align_roll_state() const -> ECheckBoxState;
    void set_align_roll_state(ECheckBoxState state);
    auto get_align_pitch_state() const -> ECheckBoxState;
    void set_align_pitch_state(ECheckBoxState state);
    auto get_align_yaw_state() const -> ECheckBoxState;
    void set_align_yaw_state(ECheckBoxState state);

    TSharedPtr<SEditableTextBox> selected_actor_name;
    FAlignAxesCheckboxStates align_axes_checkbox_states{};
    FVector layout_offset{};
};
