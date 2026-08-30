#include "SandboxNiagaraControlPanel.h"

#include "NiagaraSystem.h"

#include "AssetRegistry/AssetData.h"
#include "Editor.h"
#include "Misc/MessageDialog.h"
#include "PropertyCustomizationHelpers.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace {
TCHAR const* const default_template_path{
    TEXT("/SandboxNiagara/Templates/NS_SandboxNiagaraSeed.NS_SandboxNiagaraSeed")};

auto preset_display_name(ESandboxNiagaraExperimentPreset const preset) -> FText {
    switch (preset) {
        case ESandboxNiagaraExperimentPreset::Orbit:
            return NSLOCTEXT("SandboxNiagara", "OrbitPreset", "Orbit");
        case ESandboxNiagaraExperimentPreset::LorenzAttractor:
            return NSLOCTEXT("SandboxNiagara", "LorenzPreset", "Lorenz Attractor");
    }
    return NSLOCTEXT("SandboxNiagara", "UnknownPreset", "Unknown");
}

auto join_messages(TArray<FString> const& messages) -> FString {
    return FString::Join(messages, TEXT("\n"));
}
}

USandboxNiagaraControlPanel::USandboxNiagaraControlPanel() {
    TabDisplayName = NSLOCTEXT("SandboxNiagara", "ControlPanelTab", "Sandbox Niagara");
    bAlwaysReregisterWithWindowsMenu = true;
    template_asset_path_ = FSoftObjectPath{default_template_path};
    preset_options_.Add(MakeShared<ESandboxNiagaraExperimentPreset>(
        ESandboxNiagaraExperimentPreset::Orbit));
    preset_options_.Add(MakeShared<ESandboxNiagaraExperimentPreset>(
        ESandboxNiagaraExperimentPreset::LorenzAttractor));
}

TSharedRef<SWidget> USandboxNiagaraControlPanel::RebuildWidget() {
    auto* const subsystem{GEditor != nullptr
                              ? GEditor->GetEditorSubsystem<USandboxNiagaraSubsystem>()
                              : nullptr};
    auto const initial_name{subsystem != nullptr
                                ? subsystem->get_default_experiment_name(selected_preset_)
                                : FString{TEXT("NS_SandboxNiagaraOrbit")}};

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(12.0f)
            [SNew(SScrollBox) +
             SScrollBox::Slot()[SNew(SVerticalBox) +
                                SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
                                    [SNew(STextBlock)
                                         .Text(NSLOCTEXT("SandboxNiagara",
                                                        "ControlPanelTitle",
                                                        "Sandbox Niagara Experiments"))
                                         .Font(FAppStyle::Get().GetFontStyle(
                                             "HeadingExtraSmall"))] +
                                SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                                    [SNew(STextBlock).Text(NSLOCTEXT(
                                        "SandboxNiagara", "TemplateLabel", "Template System"))] +
                                SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
                                    [SNew(SObjectPropertyEntryBox)
                                         .AllowedClass(UNiagaraSystem::StaticClass())
                                         .ObjectPath_Lambda([this]() {
                                             return template_asset_path_.ToString();
                                         })
                                         .OnObjectChanged_UObject(
                                             this,
                                             &USandboxNiagaraControlPanel::select_template)] +
                                SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                                    [SNew(STextBlock).Text(NSLOCTEXT(
                                        "SandboxNiagara", "PresetLabel", "Experiment Preset"))] +
                                SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
                                    [SNew(SComboBox<TSharedPtr<ESandboxNiagaraExperimentPreset>>)
                                         .OptionsSource(&preset_options_)
                                         .InitiallySelectedItem(preset_options_[0])
                                         .OnSelectionChanged_UObject(
                                             this,
                                             &USandboxNiagaraControlPanel::select_preset)
                                         .OnGenerateWidget_Lambda(
                                             [](TSharedPtr<ESandboxNiagaraExperimentPreset> preset) {
                                                 return SNew(STextBlock)
                                                     .Text(preset.IsValid()
                                                               ? preset_display_name(*preset)
                                                               : FText::GetEmpty());
                                             })[SNew(STextBlock).Text_Lambda([this]() {
                                             return preset_display_name(selected_preset_);
                                         })]] +
                                SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                                    [SNew(STextBlock).Text(NSLOCTEXT(
                                        "SandboxNiagara", "NameLabel", "Experiment Name"))] +
                                SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
                                    [SAssignNew(experiment_name_input_, SEditableTextBox)
                                         .Text(FText::FromString(initial_name))] +
                                SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                                    [SNew(SUniformGridPanel)
                                         .SlotPadding(FMargin{3.0f}) +
                                     SUniformGridPanel::Slot(0, 0)
                                         [SNew(SButton)
                                              .Text(NSLOCTEXT(
                                                  "SandboxNiagara", "Generate", "Generate"))
                                              .OnClicked_UObject(
                                                  this, &USandboxNiagaraControlPanel::generate)] +
                                     SUniformGridPanel::Slot(1, 0)
                                         [SNew(SButton)
                                              .Text(NSLOCTEXT(
                                                  "SandboxNiagara", "Regenerate", "Regenerate"))
                                              .OnClicked_UObject(
                                                  this, &USandboxNiagaraControlPanel::regenerate)] +
                                     SUniformGridPanel::Slot(2, 0)
                                         [SNew(SButton)
                                              .Text(NSLOCTEXT("SandboxNiagara", "Open", "Open"))
                                              .OnClicked_UObject(
                                                  this,
                                                  &USandboxNiagaraControlPanel::open_generated_asset)] +
                                     SUniformGridPanel::Slot(0, 1)
                                         [SNew(SButton)
                                              .Text(NSLOCTEXT("SandboxNiagara",
                                                             "RegenerateAll",
                                                             "Regenerate All Presets"))
                                              .OnClicked_UObject(
                                                  this,
                                                  &USandboxNiagaraControlPanel::regenerate_all)] +
                                     SUniformGridPanel::Slot(1, 1)
                                         [SNew(SButton)
                                              .Text(NSLOCTEXT("SandboxNiagara",
                                                             "Delete",
                                                             "Delete Generated Asset"))
                                              .OnClicked_UObject(
                                                  this,
                                                  &USandboxNiagaraControlPanel::delete_generated_asset)]] +
                                SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 4.0f)
                                    [SNew(STextBlock).Text(NSLOCTEXT(
                                        "SandboxNiagara", "StatusLabel", "Status"))] +
                                SVerticalBox::Slot().AutoHeight()
                                    [SAssignNew(status_output_, SMultiLineEditableTextBox)
                                         .IsReadOnly(true)
                                         .Text(NSLOCTEXT("SandboxNiagara",
                                                        "ReadyStatus",
                                                        "Ready."))]]];
}

void USandboxNiagaraControlPanel::select_template(FAssetData const& asset_data) {
    template_asset_path_ = asset_data.ToSoftObjectPath();
}

void USandboxNiagaraControlPanel::select_preset(
    TSharedPtr<ESandboxNiagaraExperimentPreset> const preset,
    ESelectInfo::Type const selection_type) {
    if (!preset.IsValid()) {
        return;
    }
    selected_preset_ = *preset;
    if (selection_type != ESelectInfo::Direct && experiment_name_input_.IsValid() &&
        GEditor != nullptr) {
        auto* const subsystem{GEditor->GetEditorSubsystem<USandboxNiagaraSubsystem>()};
        if (subsystem != nullptr) {
            experiment_name_input_->SetText(
                FText::FromString(subsystem->get_default_experiment_name(selected_preset_)));
        }
    }
}

auto USandboxNiagaraControlPanel::generate() -> FReply {
    return run_generation(false);
}

auto USandboxNiagaraControlPanel::regenerate() -> FReply {
    return run_generation(true);
}

auto USandboxNiagaraControlPanel::run_generation(bool const replace_existing) -> FReply {
    auto* const template_system{get_template_system()};
    auto* const subsystem{GEditor != nullptr
                              ? GEditor->GetEditorSubsystem<USandboxNiagaraSubsystem>()
                              : nullptr};
    if (template_system == nullptr || subsystem == nullptr) {
        set_status(TEXT("A valid template and the Sandbox Niagara subsystem are required."),
                   false);
        return FReply::Handled();
    }

    auto const result{subsystem->generate_preset(
        template_system, selected_preset_, get_experiment_name(), replace_existing)};
    set_generation_status(result);
    return FReply::Handled();
}

auto USandboxNiagaraControlPanel::regenerate_all() -> FReply {
    auto* const template_system{get_template_system()};
    auto* const subsystem{GEditor != nullptr
                              ? GEditor->GetEditorSubsystem<USandboxNiagaraSubsystem>()
                              : nullptr};
    if (template_system == nullptr || subsystem == nullptr) {
        set_status(TEXT("A valid template and the Sandbox Niagara subsystem are required."),
                   false);
        return FReply::Handled();
    }

    set_operation_status(subsystem->regenerate_all_presets(template_system),
                         TEXT("Regenerated all preset experiments."));
    return FReply::Handled();
}

auto USandboxNiagaraControlPanel::open_generated_asset() -> FReply {
    if (GEditor == nullptr) {
        set_status(TEXT("The Unreal Editor is unavailable."), false);
        return FReply::Handled();
    }
    auto* const editor_subsystem{GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()};
    if (editor_subsystem == nullptr) {
        set_status(TEXT("The Asset Editor subsystem is unavailable."), false);
        return FReply::Handled();
    }
    auto const asset_path{get_generated_asset_path()};
    auto* const generated_asset{LoadObject<UObject>(nullptr, *asset_path, nullptr, LOAD_NoWarn)};
    if (generated_asset == nullptr) {
        set_status(FString::Printf(TEXT("Generated asset does not exist: %s"), *asset_path),
                   false);
        return FReply::Handled();
    }

    editor_subsystem->OpenEditorForAsset(generated_asset);
    set_status(FString::Printf(TEXT("Opened %s."), *asset_path), true);
    return FReply::Handled();
}

auto USandboxNiagaraControlPanel::delete_generated_asset() -> FReply {
    if (FMessageDialog::Open(EAppMsgType::YesNo,
                             FText::FromString(FString::Printf(
                                 TEXT("Delete disposable generated asset %s?"),
                                 *get_generated_asset_path()))) != EAppReturnType::Yes) {
        return FReply::Handled();
    }

    auto* const subsystem{GEditor != nullptr
                              ? GEditor->GetEditorSubsystem<USandboxNiagaraSubsystem>()
                              : nullptr};
    if (subsystem == nullptr) {
        set_status(TEXT("The Sandbox Niagara subsystem is unavailable."), false);
        return FReply::Handled();
    }
    set_operation_status(subsystem->delete_generated_asset(get_generated_asset_path()),
                         TEXT("Deleted the generated asset."));
    return FReply::Handled();
}

auto USandboxNiagaraControlPanel::get_template_system() const -> UNiagaraSystem* {
    return Cast<UNiagaraSystem>(template_asset_path_.TryLoad());
}

auto USandboxNiagaraControlPanel::get_experiment_name() const -> FString {
    return experiment_name_input_.IsValid()
               ? experiment_name_input_->GetText().ToString()
               : FString{};
}

auto USandboxNiagaraControlPanel::get_generated_asset_path() const -> FString {
    auto const name{get_experiment_name()};
    return FString::Printf(TEXT("/SandboxNiagara/Generated/%s.%s"), *name, *name);
}

void USandboxNiagaraControlPanel::set_status(FString const& message, bool const success) {
    if (status_output_.IsValid()) {
        status_output_->SetText(FText::FromString(
            FString::Printf(TEXT("%s: %s"), success ? TEXT("Success") : TEXT("Error"), *message)));
    }
}

void USandboxNiagaraControlPanel::set_generation_status(
    FSandboxNiagaraGenerationResult const& result) {
    if (result.success) {
        set_status(FString::Printf(TEXT("Generated %s (%s)."),
                                  *result.generated_asset_path,
                                  *result.compile_status),
                   true);
        return;
    }
    set_status(join_messages(result.errors), false);
}

void USandboxNiagaraControlPanel::set_operation_status(
    FSandboxNiagaraValidationResult const& result,
    FString const& success_message) {
    set_status(result.success ? success_message : join_messages(result.errors), result.success);
}
