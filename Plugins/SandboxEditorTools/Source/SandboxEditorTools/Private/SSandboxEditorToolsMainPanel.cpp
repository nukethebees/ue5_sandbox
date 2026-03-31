#include "SSandboxEditorToolsMainPanel.h"

#include "SandboxEditorToolsLogCategories.h"
#include "SandboxEditorToolsSubsystem.h"

#include "Engine/Engine.h"
#include "Selection.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SSandboxEditorToolsMainPanel::Construct(FArguments const& in_args) {
    // clang-format off
    ChildSlot
    [
        SNew(SVerticalBox)
        +SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
            .Text(FText::FromString("Hello Editor"))
		]
        +SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Center)
        [
            SNew(SButton)
            .Text(FText::FromString("Select Actor"))
            .OnClicked(this, &ThisClass::on_select_actor_button_clicked)
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
        ss->set_selected_actor(actor);
    } else {
        UE_LOG(LogSandboxEditorTools, Display, TEXT("No actors to select"));
    }

    return FReply::Handled();
}
