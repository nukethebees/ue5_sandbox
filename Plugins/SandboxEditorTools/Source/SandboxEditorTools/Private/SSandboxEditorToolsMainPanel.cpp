#include "SSandboxEditorToolsMainPanel.h"

#include "SandboxEditorToolsLogCategories.h"
#include "SandboxEditorToolsSubsystem.h"

#include "Engine/Engine.h"
#include "Selection.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SSandboxEditorToolsMainPanel::Construct(FArguments const& in_args) {
    // clang-format off
    ChildSlot
    [
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

auto SSandboxEditorToolsMainPanel::get_subsystem() -> USandboxEditorToolsSubsystem* {
    check(GEditor);
    auto* ss{GEditor->GetEditorSubsystem<USandboxEditorToolsSubsystem>()};
    return ss;
}
