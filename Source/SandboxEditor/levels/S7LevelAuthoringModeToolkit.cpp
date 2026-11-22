#include "SandboxEditor/levels/S7LevelAuthoringModeToolkit.h"

#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringMode.h"

#include <SandboxUI/slate/SlateSlots.h>

#include <IDetailsView.h>
#include <Modules/ModuleManager.h>
#include <PropertyEditorModule.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Input/SCheckBox.h>
#include <Widgets/Input/SEditableTextBox.h>
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

    auto const overlay_checkbox{
        [this](FText label, FText tooltip, bool ioj::FCollisionOverlayVisibility::* member) {
            return SNew(SCheckBox)
                .ToolTipText(tooltip)
                .IsChecked_Lambda([member] {
                    return US7LevelAuthoringMode::collision_overlays().*member
                             ? ECheckBoxState::Checked
                             : ECheckBoxState::Unchecked;
                })
                .OnCheckStateChanged_Lambda([mode = mode_, member](ECheckBoxState state) {
                    if (mode.IsValid()) {
                        auto visibility{mode->collision_overlays()};
                        visibility.*member = state == ECheckBoxState::Checked;
                        mode->set_collision_overlays(visibility);
                    }
                })[SNew(STextBlock).Text(label)];
        }};
    auto grid{overlay_checkbox(
        LOCTEXT("ShowGrid", "Show Collision Grid"),
        LOCTEXT("ShowGridTip", "Draw the level grid in the editor and during Play."),
        &ioj::FCollisionOverlayVisibility::grid)};
    auto bounds{overlay_checkbox(
        LOCTEXT("ShowBounds", "Show Collision Bounds"),
        LOCTEXT("ShowBoundsTip",
                "Draw placed entity boxes in the editor and live collision boxes during Play."),
        &ioj::FCollisionOverlayVisibility::bounds)};
    content_ =
        SNew(SScrollBox) +
        SScrollBox::Slot()[SlateGenerated::FS7LevelAuthoringModeToolkitBuilder{*this}.BuildRoot(
            details_.ToSharedRef(), grid, bounds)];

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
    if (!mode_.IsValid() || !content_.IsValid()) {
        return;
    }
    if (displayed_document_.Get() != mode_->document()) {
        displayed_document_ = mode_->document();
        details_->SetObject(displayed_document_.Get());
    }
    auto const id{mode_->selected_script_id()};
    script_id_row_->SetVisibility(id.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible);
    entity_id_->SetText(FText::FromString(id));
}

#define FORWARD_ACTION(name)                              \
    auto FS7LevelAuthoringModeToolkit::name() -> FReply { \
        if (mode_.IsValid()) {                            \
            mode_->name();                                \
        }                                                 \
        return FReply::Handled();                         \
    }

FORWARD_ACTION(create_document)
FORWARD_ACTION(link_script)
void FS7LevelAuthoringModeToolkit::commit_script_id(FText const& text,
                                                    ETextCommit::Type const commit) {
    if (commit == ETextCommit::OnEnter && mode_.IsValid()) {
        mode_->rename_selected_entity(text.ToString());
    } else {
        refresh();
    }
}
FORWARD_ACTION(open_script_editor)
FORWARD_ACTION(rebuild_scene)
FORWARD_ACTION(force_rebuild_scene)
FORWARD_ACTION(export_scene)
FORWARD_ACTION(check_scene)

#undef FORWARD_ACTION
#undef LOCTEXT_NAMESPACE
