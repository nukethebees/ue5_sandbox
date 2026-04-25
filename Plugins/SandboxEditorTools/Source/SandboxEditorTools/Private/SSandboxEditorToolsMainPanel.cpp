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
             SNew(SButton)
            .OnClicked(this, &ThisClass::on_spawn_cursor_button_clicked)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Spawn cursor")))
                    .Justification(ETextJustify::Center)
            ]
        ]
        +SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Fill)
        [
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot() 
            .AutoWidth()
            .VAlign(VAlign_Fill)
            [
                SNew(SButton)
                .OnClicked(this, &ThisClass::on_select_actor_button_clicked)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Select Actor")))
                    .Justification(ETextJustify::Center)
                ]
                
            ]
            +SHorizontalBox::Slot()
            .FillWidth(1.f)
            .VAlign(VAlign_Center)
            [
                SAssignNew(selected_actor_name, SEditableTextBox)
                .IsReadOnly(true)
            ]            
        ]
        
    ];
    // clang-format on
}
auto SSandboxEditorToolsMainPanel::on_select_actor_button_clicked() -> FReply {
    check(GEngine);
    check(GEditor);

    auto ss{GEditor->GetEditorSubsystem<USandboxEditorToolsSubsystem>()};

    auto* selected_actors{GEditor->GetSelectedActors()};
    if (selected_actors && selected_actors->Num() > 0) {
        auto* actor{Cast<AActor>(selected_actors->GetSelectedObject(0))};

        ss->set_cursor_to(actor);
        selected_actor_name->SetText(FText::FromString(actor->GetName()));
    } else {
        UE_LOG(LogSandboxEditorTools, Display, TEXT("No actors to select"));
    }

    return FReply::Handled();
}
auto SSandboxEditorToolsMainPanel::on_spawn_cursor_button_clicked() -> FReply {
    check(GEngine);
    check(GEditor);

    auto ss{GEditor->GetEditorSubsystem<USandboxEditorToolsSubsystem>()};

    if (ss) {
        ss->get_cursor();
    } else {
        UE_LOG(LogSandboxEditorTools, Error, TEXT("Tools subsystem is nullptr."));
    }

    return FReply::Handled();
}
