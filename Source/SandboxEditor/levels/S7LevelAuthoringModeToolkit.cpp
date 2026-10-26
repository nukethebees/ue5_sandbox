#include "SandboxEditor/levels/S7LevelAuthoringModeToolkit.h"

#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringMode.h"
#include "SandboxEditor/levels/S7LevelAuthoringSession.h"
#include "SandboxEditor/SandboxEditor.h"
#include <ioj/sim/collision/collision_grid.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <SandboxCoreEngine/VectorConversion.h>
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

auto FS7LevelAuthoringModeToolkit::grid_summary() const -> FText {
    return grid_summary_;
}

void FS7LevelAuthoringModeToolkit::refresh(ioj::settings::Settings const* settings) {
    if (!mode_.IsValid()) {
        return;
    }
    if (displayed_document_.Get() != mode_->document()) {
        displayed_document_ = mode_->document();
        details_->SetObject(displayed_document_.Get());
    }
    status_->SetText(mode_->status());

    auto* const document{mode_->document()};
    auto* const config{IsValid(document) ? document->level_config.Get() : nullptr};
    if (!IsValid(config)) {
        grid_summary_ = LOCTEXT("NoActiveConfig", "Level config: none");
        return;
    }

    std::expected<ioj::settings::Settings, FString> resolved{std::unexpect};
    if (!settings) {
        resolved = ioj::editor::read_s7_document_settings(*document);
        if (!resolved) {
            grid_summary_ = FText::FromString(resolved.error());
            return;
        }
        settings = &*resolved;
    }
    auto collision_grid{config->collision_grid};
    collision_grid.grid_size = ioj::to_unreal(settings->collision_grid.level_size);
    collision_grid.cell_size = ioj::to_unreal(settings->collision_grid.cell_size);
    auto const dimensions{collision_grid.calculate_grid_dimensions()};
    auto const half_bounds{ioj::to_unreal(ioj::sim::collision::grid_half_size(
        {{dimensions.X, dimensions.Y, dimensions.Z}, ioj::Vector3f{collision_grid.cell_size}}))};
    grid_summary_ =
        FText::FromString(FString::Printf(TEXT("Resources: %s | Resolved grid bounds: +/-%s cm"),
                                          *config->GetName(),
                                          *half_bounds.ToString()));
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
        mode_->save();
    }
    return FReply::Handled();
}
FORWARD_ACTION(save_as)
FORWARD_ACTION(save_canonical_from_scene)

#undef FORWARD_ACTION
#undef LOCTEXT_NAMESPACE
