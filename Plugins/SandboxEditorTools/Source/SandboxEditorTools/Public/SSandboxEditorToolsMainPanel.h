#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"

#include "SSandboxEditorToolsMainPanel.generated.h"

class STextBlock;
class SEditableTextBox;

class USandboxEditorToolsSubsystem;

struct FBool3;

USTRUCT()
struct FAlignAxesCheckboxStates {
    GENERATED_BODY()

    UPROPERTY()
    ECheckBoxState x{ECheckBoxState::Checked};
    UPROPERTY()
    ECheckBoxState y{ECheckBoxState::Checked};
    UPROPERTY()
    ECheckBoxState z{ECheckBoxState::Checked};

    auto to_bools() const -> FBool3;
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

    auto get_align_x_state() const -> ECheckBoxState;
    void set_align_x_state(ECheckBoxState state);
    auto get_align_y_state() const -> ECheckBoxState;
    void set_align_y_state(ECheckBoxState state);
    auto get_align_z_state() const -> ECheckBoxState;
    void set_align_z_state(ECheckBoxState state);

    TSharedPtr<SEditableTextBox> selected_actor_name;
    FAlignAxesCheckboxStates align_axes_checkbox_states{};
};
