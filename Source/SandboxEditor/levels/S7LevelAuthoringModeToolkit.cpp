#include "SandboxEditor/levels/S7LevelAuthoringModeToolkit.h"

#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringMode.h"

#include <SandboxUI/slate/SlateSlots.h>

#include <IDetailsView.h>
#include <Modules/ModuleManager.h>
#include <PropertyEditorModule.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Input/SEditableTextBox.h>
#include <Widgets/Input/SMultiLineEditableTextBox.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/Layout/SScrollBox.h>
#include <Widgets/Text/STextBlock.h>

#include "SandboxEditor/levels/generated/FS7LevelAuthoringModeToolkit.slate.generated.h"

#define LOCTEXT_NAMESPACE "FS7LevelAuthoringModeToolkit"

void FS7LevelAuthoringModeToolkit::Init(TSharedPtr<IToolkitHost> const& toolkit_host,
                                        TWeakObjectPtr<UEdMode> const owning_mode) {
    FModeToolkit::Init(toolkit_host, owning_mode);
    mode_ = Cast<US7LevelAuthoringMode>(owning_mode.Get());

    auto& properties{FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor")};
    FDetailsViewArgs args;
    args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
    details_ = properties.CreateDetailView(args);

    content_ =
        SNew(SScrollBox) +
        SScrollBox::Slot()[SlateGenerated::FS7LevelAuthoringModeToolkitBuilder{*this}.BuildRoot(
            details_.ToSharedRef())];

    refresh();
}

auto FS7LevelAuthoringModeToolkit::GetToolkitFName() const -> FName {
    return TEXT("SpaceGameLevelAuthoring");
}

auto FS7LevelAuthoringModeToolkit::GetBaseToolkitName() const -> FText {
    return LOCTEXT("ToolkitName", "Space Game Level Authoring");
}

auto FS7LevelAuthoringModeToolkit::GetInlineContent() const -> TSharedPtr<SWidget> {
    return content_;
}

void FS7LevelAuthoringModeToolkit::refresh() {
    if (!mode_.IsValid()) {
        return;
    }
    if (displayed_document_.Get() != mode_->document()) {
        displayed_document_ = mode_->document();
        details_->SetObject(displayed_document_.Get());
    }
    status_->SetText(mode_->status());
}

#define FORWARD_ACTION(name)                              \
    auto FS7LevelAuthoringModeToolkit::name() -> FReply { \
        if (mode_.IsValid()) {                            \
            mode_->name();                                \
        }                                                 \
        return FReply::Handled();                         \
    }

FORWARD_ACTION(create_document)
FORWARD_ACTION(open_level_config)
FORWARD_ACTION(validate_playable_level)
FORWARD_ACTION(link_script)
auto FS7LevelAuthoringModeToolkit::rename_selected_entity() -> FReply {
    if (mode_.IsValid() && entity_id_.IsValid()) {
        mode_->rename_selected_entity(entity_id_->GetText().ToString());
    }
    return FReply::Handled();
}
FORWARD_ACTION(open_script_editor)
FORWARD_ACTION(rebuild_scene)
FORWARD_ACTION(export_scene)
FORWARD_ACTION(check_export)

#undef FORWARD_ACTION
#undef LOCTEXT_NAMESPACE
