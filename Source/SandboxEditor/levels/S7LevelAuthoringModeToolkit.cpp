#include "SandboxEditor/levels/S7LevelAuthoringModeToolkit.h"

#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringMode.h"

#include <IDetailsView.h>
#include <Modules/ModuleManager.h>
#include <PropertyEditorModule.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Layout/SScrollBox.h>
#include <Widgets/Layout/SUniformGridPanel.h>
#include <Widgets/Text/STextBlock.h>

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
        SScrollBox::Slot()
            [SNew(SVerticalBox) +
             SVerticalBox::Slot().AutoHeight().Padding(2.0f)
                 [SNew(SUniformGridPanel).SlotPadding(2.0f) +
                  SUniformGridPanel::Slot(
                      0, 0)[SNew(SButton)
                                .Text(LOCTEXT("New", "New"))
                                .OnClicked(this, &FS7LevelAuthoringModeToolkit::create_document)] +
                  SUniformGridPanel::Slot(
                      1, 0)[SNew(SButton)
                                .Text(LOCTEXT("Load", "Load S7"))
                                .OnClicked(this, &FS7LevelAuthoringModeToolkit::load_s7)] +
                  SUniformGridPanel::Slot(
                      0, 1)[SNew(SButton)
                                .Text(LOCTEXT("Preview", "Preview Apply"))
                                .OnClicked(this, &FS7LevelAuthoringModeToolkit::preview_apply)] +
                  SUniformGridPanel::Slot(
                      1, 1)[SNew(SButton)
                                .Text(LOCTEXT("Apply", "Apply"))
                                .OnClicked(this, &FS7LevelAuthoringModeToolkit::apply_preview)] +
                  SUniformGridPanel::Slot(
                      0, 2)[SNew(SButton)
                                .Text(LOCTEXT("Adopt", "Adopt Actors"))
                                .OnClicked(this, &FS7LevelAuthoringModeToolkit::adopt_entities)] +
                  SUniformGridPanel::Slot(
                      1, 2)[SNew(SButton)
                                .Text(LOCTEXT("Save", "Save"))
                                .OnClicked(this, &FS7LevelAuthoringModeToolkit::save)] +
                  SUniformGridPanel::Slot(
                      0, 3)[SNew(SButton)
                                .Text(LOCTEXT("Heroes", "Set Hero"))
                                .OnClicked(this,
                                           &FS7LevelAuthoringModeToolkit::assign_selected_heroes)] +
                  SUniformGridPanel::Slot(1, 3)
                      [SNew(SButton)
                           .Text(LOCTEXT("Survive", "Set Defend"))
                           .OnClicked(
                               this, &FS7LevelAuthoringModeToolkit::assign_selected_must_survive)] +
                  SUniformGridPanel::Slot(0, 4)
                      [SNew(SButton)
                           .Text(LOCTEXT("Kills", "Set Destroy"))
                           .OnClicked(
                               this,
                               &FS7LevelAuthoringModeToolkit::assign_selected_required_kills)] +
                  SUniformGridPanel::Slot(1, 4)
                      [SNew(SButton)
                           .Text(LOCTEXT("Clear", "Clear Objective"))
                           .OnClicked(this,
                                      &FS7LevelAuthoringModeToolkit::clear_selected_objectives)] +
                  SUniformGridPanel::Slot(
                      1, 5)[SNew(SButton)
                                .Text(LOCTEXT("SaveAs", "Save As"))
                                .OnClicked(this, &FS7LevelAuthoringModeToolkit::save_as)]] +
             SVerticalBox::Slot().AutoHeight().Padding(
                 4.0f)[SAssignNew(status_, STextBlock).AutoWrapText(true)] +
             SVerticalBox::Slot().AutoHeight()[details_.ToSharedRef()]];

    if (mode_.IsValid()) {
        mode_->on_changed().AddSP(this, &FS7LevelAuthoringModeToolkit::refresh);
    }
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
    details_->SetObject(mode_->document());
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
FORWARD_ACTION(adopt_entities)
FORWARD_ACTION(assign_selected_heroes)
FORWARD_ACTION(assign_selected_must_survive)
FORWARD_ACTION(assign_selected_required_kills)
FORWARD_ACTION(clear_selected_objectives)
FORWARD_ACTION(load_s7)
FORWARD_ACTION(preview_apply)
FORWARD_ACTION(apply_preview)
FORWARD_ACTION(save)
FORWARD_ACTION(save_as)

#undef FORWARD_ACTION
#undef LOCTEXT_NAMESPACE
