#include "SandboxEditor/levels/S7LevelScriptEditor.h"

#include "SandboxEditor/levels/S7LevelAuthoringMode.h"

#include <EditorModeManager.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Input/SMultiLineEditableTextBox.h>
#include <Widgets/Layout/SBorder.h>
#include <Widgets/Layout/SScrollBox.h>
#include <Widgets/Layout/SUniformGridPanel.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/Text/STextBlock.h>

#define LOCTEXT_NAMESPACE "SS7LevelScriptEditor"

SS7LevelScriptEditor::~SS7LevelScriptEditor() {
    release_mode();
}

void SS7LevelScriptEditor::Construct(FArguments const& arguments) {
    ChildSlot[SNew(SVerticalBox) +
              SVerticalBox::Slot().AutoHeight().Padding(
                  4.0f)[SNew(SUniformGridPanel).SlotPadding(2.0f) +
                        SUniformGridPanel::Slot(
                            0, 0)[SNew(SButton)
                                      .Text(LOCTEXT("Load", "Load Source"))
                                      .IsEnabled(this, &SS7LevelScriptEditor::can_load_source)
                                      .OnClicked(this, &SS7LevelScriptEditor::load_source)] +
                        SUniformGridPanel::Slot(
                            1, 0)[SNew(SButton)
                                      .Text(LOCTEXT("Reload", "Reload/Revert"))
                                      .IsEnabled(this, &SS7LevelScriptEditor::can_reload_source)
                                      .OnClicked(this, &SS7LevelScriptEditor::reload_source)] +
                        SUniformGridPanel::Slot(
                            0, 1)[SNew(SButton)
                                      .Text(LOCTEXT("Preview", "Preview Buffer"))
                                      .IsEnabled(this, &SS7LevelScriptEditor::can_preview_buffer)
                                      .OnClicked(this, &SS7LevelScriptEditor::preview_buffer)] +
                        SUniformGridPanel::Slot(
                            1, 1)[SNew(SButton)
                                      .Text(LOCTEXT("Apply", "Apply Preview"))
                                      .IsEnabled(this, &SS7LevelScriptEditor::can_apply_preview)
                                      .OnClicked(this, &SS7LevelScriptEditor::apply_preview)] +
                        SUniformGridPanel::Slot(
                            0, 2)[SNew(SButton)
                                      .Text(LOCTEXT("Save", "Save Buffer"))
                                      .IsEnabled(this, &SS7LevelScriptEditor::can_save_buffer)
                                      .OnClicked(this, &SS7LevelScriptEditor::save_buffer)] +
                        SUniformGridPanel::Slot(
                            1, 2)[SNew(SButton)
                                      .Text(LOCTEXT("SaveAs", "Save Buffer As"))
                                      .IsEnabled(this, &SS7LevelScriptEditor::can_save_buffer)
                                      .OnClicked(this, &SS7LevelScriptEditor::save_buffer_as)] +
                        SUniformGridPanel::Slot(
                            0, 3)[SNew(SButton)
                                      .Text(LOCTEXT("SaveCanonical", "Save Canonical from Scene"))
                                      .IsEnabled(this, &SS7LevelScriptEditor::can_save_canonical)
                                      .OnClicked(this, &SS7LevelScriptEditor::save_canonical)]] +
              SVerticalBox::Slot().AutoHeight().Padding(
                  4.0f, 0.0f)[SAssignNew(state_, STextBlock).AutoWrapText(true)] +
              SVerticalBox::Slot().FillHeight(1.0f).Padding(
                  4.0f)[SAssignNew(source_, SMultiLineEditableTextBox)
                            .AutoWrapText(false)
                            .OnTextChanged(this, &SS7LevelScriptEditor::on_source_changed)] +
              SVerticalBox::Slot().AutoHeight().Padding(
                  4.0f)[SNew(SBorder)[SAssignNew(status_, STextBlock).AutoWrapText(true)]]];

    bind_active_mode();
    refresh();
}

void SS7LevelScriptEditor::Tick(FGeometry const& allotted_geometry,
                                double const current_time,
                                float const delta_time) {
    SCompoundWidget::Tick(allotted_geometry, current_time, delta_time);
    refresh_elapsed_seconds_ += delta_time;
    if (refresh_elapsed_seconds_ < 0.5f) {
        return;
    }
    refresh_elapsed_seconds_ = 0.0f;
    bind_active_mode();
    refresh();
}

void SS7LevelScriptEditor::release_mode() {
    if (mode_.IsValid() && mode_changed_handle_.IsValid()) {
        mode_->on_changed().Remove(mode_changed_handle_);
    }
    mode_changed_handle_.Reset();
    mode_.Reset();
}

void SS7LevelScriptEditor::bind_active_mode() {
    auto* const active_mode{Cast<US7LevelAuthoringMode>(
        GLevelEditorModeTools().GetActiveScriptableMode(US7LevelAuthoringMode::mode_id))};
    if (mode_.Get() == active_mode) {
        return;
    }

    release_mode();
    if (!IsValid(active_mode)) {
        return;
    }

    mode_ = active_mode;
    mode_changed_handle_ =
        active_mode->on_changed().AddSP(this, &SS7LevelScriptEditor::on_mode_changed);
}

void SS7LevelScriptEditor::refresh() {
    auto* const mode{active_mode()};
    if (!IsValid(mode)) {
        state_->SetText(LOCTEXT("Inactive", "The Space Game Level authoring mode is inactive."));
        status_->SetText(FText::GetEmpty());
        return;
    }

    auto const& buffer{mode->source_session().buffer()};
    if (source_->GetText().ToString() != buffer) {
        source_->SetText(FText::FromString(buffer));
    }
    state_->SetText(state_text());
    status_->SetText(mode->status());
}

void SS7LevelScriptEditor::on_mode_changed() {
    refresh();
}

void SS7LevelScriptEditor::on_source_changed(FText const& source) {
    if (auto* const mode{active_mode()}; IsValid(mode)) {
        mode->set_source_buffer(source.ToString());
    }
}

auto SS7LevelScriptEditor::active_mode() const -> US7LevelAuthoringMode* {
    auto* const mode{mode_.Get()};
    auto* const active_mode{Cast<US7LevelAuthoringMode>(
        GLevelEditorModeTools().GetActiveScriptableMode(US7LevelAuthoringMode::mode_id))};
    return mode == active_mode ? mode : nullptr;
}

auto SS7LevelScriptEditor::state_text() const -> FText {
    auto* const mode{active_mode()};
    if (!IsValid(mode)) {
        return LOCTEXT("Inactive", "The Space Game Level authoring mode is inactive.");
    }

    auto const state{mode->script_editor_state()};
    TArray<FString> lines;
    lines.Add(state.path.IsEmpty() ? TEXT("Path: unsaved buffer")
                                   : FString::Printf(TEXT("Path: %s"), *state.path));
    if (state.source_dirty) {
        lines.Add(TEXT("Unsaved buffer"));
    }
    if (state.unapplied_buffer) {
        lines.Add(TEXT("Unapplied buffer"));
    }
    if (state.scene_dirty) {
        lines.Add(TEXT("Scene dirty"));
    }
    if (state.disk_conflict) {
        lines.Add(TEXT("Disk conflict"));
    }
    if (state.preview_valid) {
        lines.Add(TEXT("Preview valid"));
    }
    if (state.preview_stale) {
        lines.Add(TEXT("Preview stale"));
    }
    if (state.detached) {
        lines.Add(TEXT("Detached document"));
    }
    return FText::FromString(FString::Join(lines, TEXT(" | ")));
}

auto SS7LevelScriptEditor::can_load_source() const -> bool {
    auto* const mode{active_mode()};
    return IsValid(mode) && !mode->source_session().is_dirty();
}

auto SS7LevelScriptEditor::can_reload_source() const -> bool {
    auto* const mode{active_mode()};
    return IsValid(mode) && mode->source_session().is_attached() &&
           !mode->source_session().path().IsEmpty();
}

auto SS7LevelScriptEditor::can_preview_buffer() const -> bool {
    auto* const mode{active_mode()};
    return IsValid(mode) && !mode->script_editor_state().detached;
}

auto SS7LevelScriptEditor::can_apply_preview() const -> bool {
    auto* const mode{active_mode()};
    return IsValid(mode) && mode->script_editor_state().preview_valid;
}

auto SS7LevelScriptEditor::can_save_buffer() const -> bool {
    auto* const mode{active_mode()};
    return IsValid(mode) && mode->source_session().is_attached() &&
           !mode->source_session().has_external_conflict();
}

auto SS7LevelScriptEditor::can_save_canonical() const -> bool {
    auto* const mode{active_mode()};
    if (!IsValid(mode)) {
        return false;
    }
    auto const state{mode->script_editor_state()};
    return !state.detached && !state.unapplied_buffer && !state.disk_conflict;
}

auto SS7LevelScriptEditor::load_source() -> FReply {
    if (auto* const mode{active_mode()}; IsValid(mode)) {
        mode->load_s7();
    }
    return FReply::Handled();
}

auto SS7LevelScriptEditor::reload_source() -> FReply {
    if (auto* const mode{active_mode()}; IsValid(mode)) {
        mode->reload_s7();
    }
    return FReply::Handled();
}

auto SS7LevelScriptEditor::preview_buffer() -> FReply {
    if (auto* const mode{active_mode()}; IsValid(mode)) {
        mode->preview_apply();
    }
    return FReply::Handled();
}

auto SS7LevelScriptEditor::apply_preview() -> FReply {
    if (auto* const mode{active_mode()}; IsValid(mode)) {
        mode->apply_preview();
    }
    return FReply::Handled();
}

auto SS7LevelScriptEditor::save_buffer() -> FReply {
    if (auto* const mode{active_mode()}; IsValid(mode)) {
        mode->save();
    }
    return FReply::Handled();
}

auto SS7LevelScriptEditor::save_buffer_as() -> FReply {
    if (auto* const mode{active_mode()}; IsValid(mode)) {
        mode->save_as();
    }
    return FReply::Handled();
}

auto SS7LevelScriptEditor::save_canonical() -> FReply {
    if (auto* const mode{active_mode()}; IsValid(mode)) {
        mode->save_canonical_from_scene();
    }
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
