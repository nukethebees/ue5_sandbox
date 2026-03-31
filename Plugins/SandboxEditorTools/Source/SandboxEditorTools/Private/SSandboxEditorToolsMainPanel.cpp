#include "SSandboxEditorToolsMainPanel.h"

#include "SandboxEditorToolsLogCategories.h"
#include "SandboxEditorToolsSubsystem.h"

#include "Engine/Engine.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Selection.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SSandboxEditorToolsMainPanel::Construct(FArguments const& in_args) {
    auto& edit_module{FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor")};

    // 2. Define the arguments for the view
    FDetailsViewArgs details_view_args;
    details_view_args.bAllowSearch = false;
    details_view_args.bShowOptions = true;
    details_view_args.bAllowFavoriteSystem = false;
    details_view_args.NotifyHook = nullptr;
    details_view_args.bShowOptions = true;
    details_view_args.bLockable = false;
    details_view_args.bShowObjectLabel = true;

    actor_picker = edit_module.CreateDetailView(details_view_args);

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
        .HAlign(HAlign_Left)
        [
            SNew(SButton)
            .Text(FText::FromString("Select Actor"))
            .OnClicked(this, &ThisClass::on_select_actor_button_clicked)
        ]
        +SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Fill)
        [
            actor_picker->AsShared()
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
        actor_picker->SetObject(ss);
    } else {
        UE_LOG(LogSandboxEditorTools, Display, TEXT("No actors to select"));
    }

    return FReply::Handled();
}
