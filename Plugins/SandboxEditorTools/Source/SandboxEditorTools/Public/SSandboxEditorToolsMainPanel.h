#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"

#include "SSandboxEditorToolsMainPanel.generated.h"

class STextBlock;
class SEditableTextBox;

class USandboxEditorToolsSubsystem;

struct FRotationBool;

USTRUCT()
struct FAlignAxesCheckboxStates {
    GENERATED_BODY()

    UPROPERTY()
    ECheckBoxState pitch{ECheckBoxState::Unchecked};
    UPROPERTY()
    ECheckBoxState yaw{ECheckBoxState::Checked};
    UPROPERTY()
    ECheckBoxState roll{ECheckBoxState::Unchecked};

    auto to_bools() const -> FRotationBool;
};

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

    auto get_align_roll_state() const -> ECheckBoxState;
    void set_align_roll_state(ECheckBoxState state);
    auto get_align_pitch_state() const -> ECheckBoxState;
    void set_align_pitch_state(ECheckBoxState state);
    auto get_align_yaw_state() const -> ECheckBoxState;
    void set_align_yaw_state(ECheckBoxState state);

    TSharedPtr<SEditableTextBox> selected_actor_name;
    FAlignAxesCheckboxStates align_axes_checkbox_states{};
};
