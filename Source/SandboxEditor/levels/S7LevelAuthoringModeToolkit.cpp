#include "SandboxEditor/levels/S7LevelAuthoringModeToolkit.h"

#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringMode.h"
#include "SandboxEditor/SandboxEditor.h"

#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <IDetailsView.h>
#include <Modules/ModuleManager.h>
#include <PropertyEditorModule.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Input/SEditableTextBox.h>
#include <Widgets/Input/SMultiLineEditableTextBox.h>
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
                                .Text(LOCTEXT("Load", "Load & Apply S7"))
                                .OnClicked(this, &FS7LevelAuthoringModeToolkit::load_s7)] +
                  SUniformGridPanel::Slot(
                      0, 1)[SNew(SButton)
                                .Text(LOCTEXT("Preview", "Preview Apply"))
                                .OnClicked(this, &FS7LevelAuthoringModeToolkit::preview_apply)] +
                  SUniformGridPanel::Slot(
                      1, 1)[SNew(SButton)
                                .Text(LOCTEXT("Apply", "Apply Preview"))
                                .OnClicked(this, &FS7LevelAuthoringModeToolkit::apply_preview)] +
                  SUniformGridPanel::Slot(0, 2)
                      [SNew(SButton)
                           .Text(LOCTEXT("RepairAndAdopt", "Repair / Adopt Actors"))
                           .OnClicked(this,
                                      &FS7LevelAuthoringModeToolkit::repair_and_adopt_entities)] +
                  SUniformGridPanel::Slot(
                      1, 2)[SNew(SButton)
                                .Text(LOCTEXT("Save", "Save Source"))
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
                      0, 5)[SNew(SButton)
                                .Text(LOCTEXT("SaveAs", "Save Source As"))
                                .OnClicked(this, &FS7LevelAuthoringModeToolkit::save_as)] +
                  SUniformGridPanel::Slot(1, 5)
                      [SNew(SButton)
                           .Text(LOCTEXT("SaveCanonical", "Save Canonical from Scene"))
                           .OnClicked(this,
                                      &FS7LevelAuthoringModeToolkit::save_canonical_from_scene)] +
                  SUniformGridPanel::Slot(0, 6)
                      [SNew(SButton)
                           .Text(LOCTEXT("OpenScriptEditor", "Open Script Editor"))
                           .OnClicked(this, &FS7LevelAuthoringModeToolkit::open_script_editor)] +
                  SUniformGridPanel::Slot(1, 6)
                      [SNew(SButton)
                           .Text(LOCTEXT("ImportMission", "Import Orchestrator Mission"))
                           .OnClicked(this,
                                      &FS7LevelAuthoringModeToolkit::import_orchestrator_mission)] +
                  SUniformGridPanel::Slot(0, 7)
                      [SNew(SButton)
                           .Text(LOCTEXT("SetUpPlayableLevel", "Set Up Playable Level"))
                           .OnClicked(this, &FS7LevelAuthoringModeToolkit::set_up_playable_level)] +
                  SUniformGridPanel::Slot(
                      1, 7)[SNew(SButton)
                                .Text(LOCTEXT("ValidatePlayableLevel", "Validate Level"))
                                .OnClicked(
                                    this, &FS7LevelAuthoringModeToolkit::validate_playable_level)] +
                  SUniformGridPanel::Slot(0, 8)
                      [SNew(SButton)
                           .Text(LOCTEXT("EditLevelConfig", "Edit Grid Defaults"))
                           .ToolTipText(LOCTEXT(
                               "EditLevelConfigTooltip",
                               "Edit shared defaults. Changes may affect multiple S7 levels."))
                           .OnClicked(this, &FS7LevelAuthoringModeToolkit::open_level_config)] +
                  SUniformGridPanel::Slot(
                      0, 9)[SAssignNew(entity_id_, SEditableTextBox)
                                .HintText(LOCTEXT("EntityIdHint", "Selected entity ID"))] +
                  SUniformGridPanel::Slot(
                      1, 9)[SNew(SButton)
                                .Text(LOCTEXT("RenameEntity", "Rename Entity ID"))
                                .OnClicked(
                                    this, &FS7LevelAuthoringModeToolkit::rename_selected_entity)]] +
             SVerticalBox::Slot().AutoHeight().Padding(
                 4.0f)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] {
                 auto* const document{mode_.IsValid() ? mode_->document() : nullptr};
                 auto* const config{IsValid(document) ? document->level_config.Get() : nullptr};
                 if (!IsValid(config)) {
                     return LOCTEXT("NoActiveConfig", "Level config: none");
                 }
                 auto const collision_grid{
                     document->resolve_collision_grid(config->collision_grid)};
                 auto const dimensions{collision_grid.calculate_grid_dimensions()};
                 auto const cell_size{collision_grid.cell_size};
                 auto const half_bounds{FVector3f{dimensions.X * cell_size.X * 0.5f,
                                                  dimensions.Y * cell_size.Y * 0.5f,
                                                  dimensions.Z * cell_size.Z * 0.5f}};
                 return FText::FromString(
                     FString::Printf(TEXT("Shared grid defaults: %s | Effective grid bounds: "
                                          "+/-%s cm"),
                                     *config->GetName(),
                                     *half_bounds.ToString()));
             })] +
             SVerticalBox::Slot().AutoHeight().Padding(
                 4.0f)[SAssignNew(status_, SMultiLineEditableTextBox)
                           .IsReadOnly(true)
                           .AutoWrapText(true)] +
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
FORWARD_ACTION(repair_and_adopt_entities)
FORWARD_ACTION(assign_selected_heroes)
FORWARD_ACTION(assign_selected_must_survive)
FORWARD_ACTION(assign_selected_required_kills)
FORWARD_ACTION(clear_selected_objectives)
FORWARD_ACTION(import_orchestrator_mission)
FORWARD_ACTION(open_level_config)
FORWARD_ACTION(validate_playable_level)
FORWARD_ACTION(set_up_playable_level)
auto FS7LevelAuthoringModeToolkit::load_s7() -> FReply {
    if (mode_.IsValid() && mode_->load_s7()) {
        mode_->apply_preview();
    }
    return FReply::Handled();
}
auto FS7LevelAuthoringModeToolkit::rename_selected_entity() -> FReply {
    if (mode_.IsValid() && entity_id_.IsValid()) {
        mode_->rename_selected_entity(entity_id_->GetText().ToString());
    }
    return FReply::Handled();
}
auto FS7LevelAuthoringModeToolkit::open_script_editor() -> FReply {
    FSandboxEditorModule::open_s7_level_script_editor();
    return FReply::Handled();
}
FORWARD_ACTION(preview_apply)
FORWARD_ACTION(apply_preview)
auto FS7LevelAuthoringModeToolkit::save() -> FReply {
    if (mode_.IsValid()) {
        if (mode_->source_session().buffer().TrimStartAndEnd().IsEmpty()) {
            mode_->save_canonical_from_scene();
        } else {
            mode_->save();
        }
    }
    return FReply::Handled();
}
FORWARD_ACTION(save_as)
FORWARD_ACTION(save_canonical_from_scene)

#undef FORWARD_ACTION
#undef LOCTEXT_NAMESPACE
