#include "SandboxNiagaraControlPanel.h"
#include "SandboxNiagaraPreviewViewport.h"

#include "SandboxUI/slate/SlateSlots.h"
#include "SandboxUI/widgets/SLabeledRow.h"

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
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace {
TCHAR const* const default_template_path{
    TEXT("/SandboxNiagara/Templates/NS_SandboxNiagaraSeed.NS_SandboxNiagaraSeed")};

auto join_messages(TArray<FString> const& messages) -> FString {
    return FString::Join(messages, TEXT("\n"));
}
}

USandboxNiagaraControlPanel::USandboxNiagaraControlPanel() {
    TabDisplayName = NSLOCTEXT("SandboxNiagara", "ControlPanelTab", "Sandbox Niagara");
    bAlwaysReregisterWithWindowsMenu = true;
    template_asset_path_ = FSoftObjectPath{default_template_path};
}

TSharedRef<SWidget> USandboxNiagaraControlPanel::RebuildWidget() {
    auto* const subsystem{GEditor != nullptr
                              ? GEditor->GetEditorSubsystem<USandboxNiagaraSubsystem>()
                              : nullptr};
    auto const previous_id{selected_experiment_.IsValid() ? selected_experiment_->id : FString{}};
    experiment_options_.Reset();
    auto const catalog{subsystem != nullptr ? subsystem->load_experiment_catalog()
                                            : FSandboxNiagaraExperimentCatalogResult{}};
    for (FSandboxNiagaraExperimentDefinition const& experiment : catalog.experiments) {
        experiment_options_.Add(MakeShared<FSandboxNiagaraExperimentDefinition>(experiment));
    }
    auto const* previous_experiment{experiment_options_.FindByPredicate(
        [&previous_id](TSharedPtr<FSandboxNiagaraExperimentDefinition> const& experiment) {
            return experiment.IsValid() && experiment->id == previous_id;
        })};
    selected_experiment_ =
        previous_experiment != nullptr ? *previous_experiment : nullptr;
    if (!selected_experiment_.IsValid() && !experiment_options_.IsEmpty()) {
        selected_experiment_ = experiment_options_[0];
    }
    if (selected_experiment_.IsValid()) {
        selected_configuration_ = selected_experiment_->configuration;
    }

    auto const initial_name{selected_experiment_.IsValid()
                                ? selected_experiment_->default_asset_name
                                : FString{TEXT("NS_SandboxNiagaraExperiment")}};
    auto const initial_status{catalog.success
                                  ? FString::Printf(TEXT("Loaded %d text-defined experiment(s)."),
                                                    catalog.experiments.Num())
                                  : join_messages(catalog.errors)};
    SAssignNew(preview_viewport_, SSandboxNiagaraPreviewViewport);
    auto const initial_asset_path{FString::Printf(
        TEXT("/SandboxNiagara/Generated/%s.%s"), *initial_name, *initial_name)};
    auto* initial_system{
        LoadObject<UNiagaraSystem>(nullptr, *initial_asset_path, nullptr, LOAD_NoWarn)};
    if (initial_system == nullptr) {
        initial_system = Cast<UNiagaraSystem>(template_asset_path_.TryLoad());
    }
    preview_viewport_->set_system(initial_system);

    auto root{SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(12.0f)
            [SNew(SScrollBox) +
             SScrollBox::Slot()[SNew(SVerticalBox) +
                                SandboxUI::Slate::vbox_auto_slot(
                                    FMargin{0.0f, 0.0f, 0.0f, 12.0f})
                                    [SNew(STextBlock)
                                         .Text(NSLOCTEXT("SandboxNiagara",
                                                        "ControlPanelTitle",
                                                        "Sandbox Niagara Experiments"))
                                         .Font(FAppStyle::Get().GetFontStyle(
                                             "HeadingExtraSmall"))] +
                                SandboxUI::Slate::vbox_auto_slot(
                                    FMargin{0.0f, 0.0f, 0.0f, 8.0f})
                                    [SNew(STextBlock).Text(NSLOCTEXT(
                                        "SandboxNiagara", "TemplateLabel", "Template System"))] +
                                SandboxUI::Slate::vbox_auto_slot(
                                    FMargin{0.0f, 0.0f, 0.0f, 12.0f})
                                    [SNew(SObjectPropertyEntryBox)
                                         .AllowedClass(UNiagaraSystem::StaticClass())
                                         .ObjectPath_Lambda([this]() {
                                             return template_asset_path_.ToString();
                                         })
                                         .OnObjectChanged_UObject(
                                             this,
                                             &USandboxNiagaraControlPanel::select_template)] +
                                SandboxUI::Slate::vbox_auto_slot(
                                    FMargin{0.0f, 0.0f, 0.0f, 8.0f})
                                    [SNew(STextBlock).Text(NSLOCTEXT(
                                        "SandboxNiagara", "ExperimentLabel", "Experiment"))] +
                                SandboxUI::Slate::vbox_auto_slot(
                                    FMargin{0.0f, 0.0f, 0.0f, 12.0f})
                                    [SAssignNew(
                                         experiment_combo_,
                                         SComboBox<TSharedPtr<FSandboxNiagaraExperimentDefinition>>)
                                         .OptionsSource(&experiment_options_)
                                         .InitiallySelectedItem(selected_experiment_)
                                         .OnSelectionChanged_UObject(
                                             this,
                                             &USandboxNiagaraControlPanel::select_experiment)
                                         .OnGenerateWidget_Lambda(
                                             [](TSharedPtr<FSandboxNiagaraExperimentDefinition>
                                                    experiment) {
                                                 return SNew(STextBlock)
                                                     .Text(experiment.IsValid()
                                                               ? FText::FromString(
                                                                     experiment->display_name)
                                                               : FText::GetEmpty());
                                             })[SNew(STextBlock).Text_Lambda([this]() {
                                             return selected_experiment_.IsValid()
                                                        ? FText::FromString(
                                                              selected_experiment_->display_name)
                                                        : NSLOCTEXT("SandboxNiagara",
                                                                   "NoExperiments",
                                                                   "No experiments found");
                                         })]] +
                                SandboxUI::Slate::vbox_auto_slot(
                                    FMargin{0.0f, 0.0f, 0.0f, 8.0f})
                                    [SNew(STextBlock).Text(NSLOCTEXT(
                                        "SandboxNiagara", "NameLabel", "Experiment Name"))] +
                                SandboxUI::Slate::vbox_auto_slot(
                                    FMargin{0.0f, 0.0f, 0.0f, 12.0f})
                                    [SAssignNew(experiment_name_input_, SEditableTextBox)
                                         .Text(FText::FromString(initial_name))] +
                                SandboxUI::Slate::vbox_auto_slot(
                                    FMargin{0.0f, 0.0f, 0.0f, 8.0f})
                                    [SNew(STextBlock).Text(NSLOCTEXT(
                                        "SandboxNiagara", "ParametersLabel", "Parameters"))] +
                                SandboxUI::Slate::vbox_auto_slot(
                                    FMargin{0.0f, 0.0f, 0.0f, 12.0f})
                                    [SAssignNew(parameter_list_, SVerticalBox)] +
                                SandboxUI::Slate::vbox_auto_slot(
                                    FMargin{0.0f, 0.0f, 0.0f, 8.0f})
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
                                                             "Regenerate All Experiments"))
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
                                                  &USandboxNiagaraControlPanel::delete_generated_asset)] +
                                     SUniformGridPanel::Slot(2, 1)
                                         [SNew(SButton)
                                              .Text(NSLOCTEXT("SandboxNiagara",
                                                             "ReloadDefinitions",
                                                             "Reload Definitions"))
                                              .OnClicked_UObject(
                                                  this,
                                                  &USandboxNiagaraControlPanel::reload_definitions)] +
                                     SUniformGridPanel::Slot(0, 2)
                                         [SNew(SButton)
                                              .Text(NSLOCTEXT("SandboxNiagara",
                                                             "CreateShowcase",
                                                             "Create/Refresh Showcase"))
                                              .OnClicked_UObject(
                                                  this,
                                                  &USandboxNiagaraControlPanel::create_or_refresh_showcase)] +
                                     SUniformGridPanel::Slot(1, 2)
                                         [SNew(SButton)
                                              .Text(NSLOCTEXT("SandboxNiagara",
                                                             "OpenShowcase",
                                                             "Open Showcase"))
                                              .OnClicked_UObject(
                                                  this,
                                                  &USandboxNiagaraControlPanel::open_showcase)]] +
                                SandboxUI::Slate::vbox_auto_slot(
                                    FMargin{0.0f, 8.0f, 0.0f, 8.0f})
                                    [SNew(STextBlock).Text(NSLOCTEXT(
                                        "SandboxNiagara", "PreviewLabel", "Live Preview"))] +
                                SandboxUI::Slate::vbox_auto_slot(
                                    FMargin{0.0f, 0.0f, 0.0f, 8.0f})
                                    [SNew(SUniformGridPanel)
                                         .SlotPadding(FMargin{3.0f}) +
                                     SUniformGridPanel::Slot(0, 0)
                                         [SNew(SButton)
                                              .Text(NSLOCTEXT("SandboxNiagara",
                                                             "RestartPreview",
                                                             "Restart Preview"))
                                              .OnClicked_UObject(
                                                  this,
                                                  &USandboxNiagaraControlPanel::restart_preview)] +
                                     SUniformGridPanel::Slot(1, 0)
                                         [SNew(SButton)
                                              .Text_Lambda([this]() {
                                                  return get_pause_button_text();
                                              })
                                              .OnClicked_UObject(
                                                  this,
                                                  &USandboxNiagaraControlPanel::toggle_preview_paused)] +
                                     SUniformGridPanel::Slot(2, 0)
                                         [SNew(SButton)
                                              .Text(NSLOCTEXT("SandboxNiagara",
                                                             "FramePreview",
                                                             "Frame Effect"))
                                              .OnClicked_UObject(
                                                  this,
                                                  &USandboxNiagaraControlPanel::frame_preview)]] +
                                SandboxUI::Slate::vbox_auto_slot(
                                    FMargin{0.0f, 0.0f, 0.0f, 8.0f})
                                    [SNew(SBox)
                                         .HeightOverride(520.0f)
                                         [preview_viewport_.ToSharedRef()]] +
                                SandboxUI::Slate::vbox_auto_slot(
                                    FMargin{0.0f, 8.0f, 0.0f, 4.0f})
                                    [SNew(STextBlock).Text(NSLOCTEXT(
                                        "SandboxNiagara", "StatusLabel", "Status"))] +
                                SandboxUI::Slate::vbox_auto_slot()
                                    [SAssignNew(status_output_, SMultiLineEditableTextBox)
                                         .IsReadOnly(true)
                                         .Text(FText::FromString(initial_status))]]]};
    rebuild_parameter_controls();
    return root;
}

void USandboxNiagaraControlPanel::select_template(FAssetData const& asset_data) {
    template_asset_path_ = asset_data.ToSoftObjectPath();
}

void USandboxNiagaraControlPanel::select_experiment(
    TSharedPtr<FSandboxNiagaraExperimentDefinition> const experiment,
    ESelectInfo::Type const selection_type) {
    if (!experiment.IsValid()) {
        return;
    }
    selected_experiment_ = experiment;
    selected_configuration_ = experiment->configuration;
    rebuild_parameter_controls();
    if (selection_type != ESelectInfo::Direct && experiment_name_input_.IsValid()) {
        experiment_name_input_->SetText(FText::FromString(experiment->default_asset_name));
        refresh_preview();
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
    if (template_system == nullptr || subsystem == nullptr ||
        !selected_experiment_.IsValid()) {
        set_status(TEXT("A valid template, text-defined experiment, and the Sandbox Niagara "
                        "subsystem are required."),
                   false);
        return FReply::Handled();
    }

    auto const result{replace_existing
                          ? subsystem->regenerate_experiment(template_system,
                                                             get_experiment_name(),
                                                             selected_configuration_)
                          : subsystem->generate_experiment(template_system,
                                                           get_experiment_name(),
                                                           selected_configuration_)};
    set_generation_status(result);
    if (result.success && preview_viewport_.IsValid()) {
        preview_viewport_->set_system(result.generated_system);
    }
    return FReply::Handled();
}

auto USandboxNiagaraControlPanel::get_float_parameter(FName const parameter_name) const
    -> TOptional<float> {
    auto const* parameter{selected_configuration_.float_parameters.FindByPredicate(
        [parameter_name](FSandboxNiagaraFloatParameter const& candidate) {
            return candidate.name == parameter_name;
        })};
    return parameter != nullptr ? TOptional<float>{parameter->value} : TOptional<float>{};
}

void USandboxNiagaraControlPanel::set_float_parameter(FName const parameter_name,
                                                      float const value) {
    auto* const parameter{selected_configuration_.float_parameters.FindByPredicate(
        [parameter_name](FSandboxNiagaraFloatParameter const& candidate) {
            return candidate.name == parameter_name;
        })};
    if (parameter != nullptr) {
        parameter->value = value;
    }
}

void USandboxNiagaraControlPanel::rebuild_parameter_controls() {
    if (!parameter_list_.IsValid()) {
        return;
    }
    parameter_list_->ClearChildren();

    struct FConfigurationParameter {
        FText label{};
        float FSandboxNiagaraExperimentConfiguration::* member{nullptr};
        float minimum{0.0f};
        float maximum{0.0f};
    };
    TArray<FConfigurationParameter> configuration_parameters{
        {NSLOCTEXT("SandboxNiagara", "LifetimeParameter", "Particle Lifetime"),
         &FSandboxNiagaraExperimentConfiguration::particle_lifetime,
         0.001f,
         100000.0f},
        {NSLOCTEXT("SandboxNiagara", "SpriteSizeParameter", "Sprite Size"),
         &FSandboxNiagaraExperimentConfiguration::sprite_size,
         0.001f,
         100000.0f},
        {NSLOCTEXT("SandboxNiagara", "SpawnRadiusParameter", "Spawn Radius"),
         &FSandboxNiagaraExperimentConfiguration::spawn_radius,
         0.0f,
         1000000.0f},
    };
    if (selected_configuration_.emission_mode == ESandboxNiagaraEmissionMode::Continuous) {
        configuration_parameters.Insert(
            {NSLOCTEXT("SandboxNiagara", "SpawnRateParameter", "Spawn Rate"),
             &FSandboxNiagaraExperimentConfiguration::spawn_rate,
             0.0f,
             1000000.0f},
            0);
    } else {
        parameter_list_->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
            [SNew(SLabeledRow)
                 .Label(NSLOCTEXT("SandboxNiagara", "BurstCountParameter", "Burst Count"))
                 .LabelFillWidth(0.45f)
                 .ContentFillWidth(0.55f)
                     [SNew(SNumericEntryBox<int32>)
                          .MinValue(1)
                          .MaxValue(1000000)
                          .Value_Lambda([this]() {
                              return TOptional<int32>{selected_configuration_.burst_count};
                          })
                          .OnValueCommitted_Lambda([this](int32 const value, ETextCommit::Type) {
                              selected_configuration_.burst_count = value;
                          })]];
    }
    if (selected_configuration_.spawn_shape == ESandboxNiagaraSpawnShape::Cylinder) {
        configuration_parameters.Add(
            {NSLOCTEXT("SandboxNiagara", "SpawnHeightParameter", "Spawn Height"),
             &FSandboxNiagaraExperimentConfiguration::spawn_height,
             0.001f,
             1000000.0f});
    }
    if (selected_configuration_.spawn_shape == ESandboxNiagaraSpawnShape::Ring) {
        configuration_parameters.Add(
            {NSLOCTEXT("SandboxNiagara", "SpawnInnerRadiusParameter", "Spawn Inner Radius"),
             &FSandboxNiagaraExperimentConfiguration::spawn_inner_radius,
             0.0f,
             selected_configuration_.spawn_radius});
    }
    configuration_parameters.Add(
        {NSLOCTEXT("SandboxNiagara", "BoundsParameter", "Bounds Extent"),
         &FSandboxNiagaraExperimentConfiguration::fixed_bounds_extent,
         0.001f,
         10000000.0f});

    for (FConfigurationParameter const& parameter : configuration_parameters) {
        auto const member{parameter.member};
        parameter_list_->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
            [SNew(SLabeledRow)
                 .Label(parameter.label)
                 .LabelFillWidth(0.45f)
                 .ContentFillWidth(0.55f)
                     [SNew(SNumericEntryBox<float>)
                          .MinValue(parameter.minimum)
                          .MaxValue(parameter.maximum)
                          .Value_Lambda([this, member]() {
                              return TOptional<float>{selected_configuration_.*member};
                          })
                          .OnValueCommitted_Lambda(
                              [this, member](float const value, ETextCommit::Type) {
                                  selected_configuration_.*member = value;
                              })]];
    }

    for (FSandboxNiagaraFloatParameter const& parameter :
         selected_configuration_.float_parameters) {
        auto const parameter_name{parameter.name};
        parameter_list_->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
            [SNew(SLabeledRow)
                 .Label(FText::FromString(parameter.display_name))
                 .LabelFillWidth(0.45f)
                 .ContentFillWidth(0.55f)
                     [SNew(SNumericEntryBox<float>)
                          .MinValue(parameter.minimum)
                          .MaxValue(parameter.maximum)
                          .Value_Lambda([this, parameter_name]() {
                              return get_float_parameter(parameter_name);
                          })
                          .OnValueCommitted_Lambda(
                              [this, parameter_name](float const value, ETextCommit::Type) {
                                  set_float_parameter(parameter_name, value);
                              })]];
    }
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

    auto const result{subsystem->regenerate_all_experiments(template_system)};
    set_operation_status(result, TEXT("Regenerated all text-defined experiments."));
    if (result.success) {
        refresh_preview();
    }
    return FReply::Handled();
}

auto USandboxNiagaraControlPanel::create_or_refresh_showcase() -> FReply {
    auto* const subsystem{GEditor != nullptr
                              ? GEditor->GetEditorSubsystem<USandboxNiagaraSubsystem>()
                              : nullptr};
    if (subsystem == nullptr) {
        set_status(TEXT("The Sandbox Niagara subsystem is unavailable."), false);
        return FReply::Handled();
    }

    auto const result{subsystem->create_or_refresh_showcase()};
    set_operation_status(result, TEXT("Created and opened the Niagara showcase."));
    if (result.success) {
        refresh_preview();
    }
    return FReply::Handled();
}

auto USandboxNiagaraControlPanel::open_showcase() -> FReply {
    auto* const subsystem{GEditor != nullptr
                              ? GEditor->GetEditorSubsystem<USandboxNiagaraSubsystem>()
                              : nullptr};
    if (subsystem == nullptr) {
        set_status(TEXT("The Sandbox Niagara subsystem is unavailable."), false);
        return FReply::Handled();
    }

    auto const result{subsystem->open_showcase()};
    set_operation_status(result, TEXT("Opened the Niagara showcase."));
    return FReply::Handled();
}

auto USandboxNiagaraControlPanel::reload_definitions() -> FReply {
    auto* const subsystem{GEditor != nullptr
                              ? GEditor->GetEditorSubsystem<USandboxNiagaraSubsystem>()
                              : nullptr};
    if (subsystem == nullptr) {
        set_status(TEXT("The Sandbox Niagara subsystem is unavailable."), false);
        return FReply::Handled();
    }

    auto const catalog{subsystem->load_experiment_catalog()};
    if (!catalog.success) {
        set_status(join_messages(catalog.errors), false);
        return FReply::Handled();
    }

    auto const selected_id{selected_experiment_.IsValid() ? selected_experiment_->id : FString{}};
    experiment_options_.Reset();
    for (FSandboxNiagaraExperimentDefinition const& experiment : catalog.experiments) {
        experiment_options_.Add(MakeShared<FSandboxNiagaraExperimentDefinition>(experiment));
    }
    auto const* selected_option{experiment_options_.FindByPredicate(
        [&selected_id](TSharedPtr<FSandboxNiagaraExperimentDefinition> const& experiment) {
            return experiment.IsValid() && experiment->id == selected_id;
        })};
    selected_experiment_ = selected_option != nullptr ? *selected_option : experiment_options_[0];
    selected_configuration_ = selected_experiment_->configuration;
    if (experiment_combo_.IsValid()) {
        experiment_combo_->RefreshOptions();
        experiment_combo_->SetSelectedItem(selected_experiment_);
    }
    if (experiment_name_input_.IsValid()) {
        experiment_name_input_->SetText(
            FText::FromString(selected_experiment_->default_asset_name));
    }
    rebuild_parameter_controls();
    refresh_preview();
    set_status(FString::Printf(TEXT("Reloaded %d text-defined experiment(s)."),
                               catalog.experiments.Num()),
               true);
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
    auto const result{subsystem->delete_generated_asset(get_generated_asset_path())};
    set_operation_status(result, TEXT("Deleted the generated asset."));
    if (result.success) {
        refresh_preview();
    }
    return FReply::Handled();
}

auto USandboxNiagaraControlPanel::restart_preview() -> FReply {
    if (preview_viewport_.IsValid()) {
        preview_viewport_->restart();
    }
    return FReply::Handled();
}

auto USandboxNiagaraControlPanel::frame_preview() -> FReply {
    if (preview_viewport_.IsValid()) {
        preview_viewport_->frame_system();
    }
    return FReply::Handled();
}

auto USandboxNiagaraControlPanel::toggle_preview_paused() -> FReply {
    if (preview_viewport_.IsValid()) {
        preview_viewport_->toggle_paused();
    }
    return FReply::Handled();
}

auto USandboxNiagaraControlPanel::get_pause_button_text() const -> FText {
    return preview_viewport_.IsValid() && preview_viewport_->is_paused()
               ? NSLOCTEXT("SandboxNiagara", "ResumePreview", "Resume Preview")
               : NSLOCTEXT("SandboxNiagara", "PausePreview", "Pause Preview");
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

void USandboxNiagaraControlPanel::refresh_preview() {
    if (!preview_viewport_.IsValid()) {
        return;
    }

    auto* system{LoadObject<UNiagaraSystem>(
        nullptr, *get_generated_asset_path(), nullptr, LOAD_NoWarn)};
    if (system == nullptr) {
        system = get_template_system();
    }
    preview_viewport_->set_system(system);
}
