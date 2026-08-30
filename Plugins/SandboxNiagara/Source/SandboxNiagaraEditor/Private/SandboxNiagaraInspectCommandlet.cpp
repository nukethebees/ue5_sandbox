#include "SandboxNiagaraInspectCommandlet.h"

#include "SandboxNiagaraControlPanel.h"
#include "SandboxNiagaraSubsystem.h"

#include "NiagaraExternalSystemEditorUtilities.h"
#include "NiagaraSystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "EditorUtilityWidgetBlueprintFactory.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Parse.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "Components/CanvasPanel.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogSandboxNiagaraInspect, Log, All);

namespace {
TCHAR const* const seed_asset_path{
    TEXT("/SandboxNiagara/Templates/NS_SandboxNiagaraSeed.NS_SandboxNiagaraSeed")};
TCHAR const* const control_panel_package_path{
    TEXT("/SandboxNiagara/Editor/EUW_SandboxNiagaraControlPanel")};
TCHAR const* const control_panel_asset_name{TEXT("EUW_SandboxNiagaraControlPanel")};

auto create_control_panel_asset() -> bool {
    auto* const existing{LoadObject<UEditorUtilityWidgetBlueprint>(
        nullptr,
        TEXT("/SandboxNiagara/Editor/EUW_SandboxNiagaraControlPanel."
             "EUW_SandboxNiagaraControlPanel"))};
    if (existing != nullptr) {
        auto const valid_parent{existing->GeneratedClass != nullptr &&
                                existing->GeneratedClass->IsChildOf(
                                    USandboxNiagaraControlPanel::StaticClass())};
        if (!valid_parent) {
            UE_LOG(LogSandboxNiagaraInspect,
                   Error,
                   TEXT("Existing control panel asset has the wrong parent class."));
        }
        return valid_parent;
    }

    auto* const package{CreatePackage(control_panel_package_path)};
    auto* const factory{NewObject<UEditorUtilityWidgetBlueprintFactory>()};
    factory->ParentClass = USandboxNiagaraControlPanel::StaticClass();
    factory->RootWidgetClass = UCanvasPanel::StaticClass();
    auto* const widget_blueprint{Cast<UEditorUtilityWidgetBlueprint>(factory->FactoryCreateNew(
        UEditorUtilityWidgetBlueprint::StaticClass(),
        package,
        control_panel_asset_name,
        RF_Public | RF_Standalone | RF_Transactional,
        nullptr,
        GWarn))};
    if (widget_blueprint == nullptr || widget_blueprint->Status == BS_Error || GEditor == nullptr) {
        UE_LOG(LogSandboxNiagaraInspect,
               Error,
               TEXT("Failed to create the Sandbox Niagara control panel asset."));
        return false;
    }

    FKismetEditorUtilities::CompileBlueprint(widget_blueprint);
    FAssetRegistryModule::AssetCreated(widget_blueprint);
    widget_blueprint->MarkPackageDirty();

    auto* const asset_subsystem{GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()};
    if (asset_subsystem == nullptr ||
        !asset_subsystem->SaveLoadedAsset(widget_blueprint, false)) {
        UE_LOG(LogSandboxNiagaraInspect,
               Error,
               TEXT("Failed to save the Sandbox Niagara control panel asset."));
        return false;
    }

    UE_LOG(LogSandboxNiagaraInspect,
           Display,
           TEXT("Created control panel asset %s.%s."),
           control_panel_package_path,
           control_panel_asset_name);
    return true;
}

void log_stack(FNiagaraExt_ScriptStackTopology const& stack) {
    UE_LOG(LogSandboxNiagaraInspect,
           Display,
           TEXT("  Stack %s (%d modules)"),
           *stack.ScriptName.ToString(),
           stack.Modules.Num());

    for (FNiagaraExt_ModuleTopology const& module : stack.Modules) {
        UE_LOG(LogSandboxNiagaraInspect,
               Display,
               TEXT("    Module %s (%d inputs)"),
               *module.ModuleName.ToString(),
               module.Inputs.Num());
        for (FNiagaraExt_StackInputTopology const& input : module.Inputs) {
            UE_LOG(LogSandboxNiagaraInspect,
                   Display,
                   TEXT("      Input %s [%s] visible=%s editable=%s expression=%s"),
                   *input.Name.ToString(),
                   *input.Type.GetName(),
                   input.bIsVisible ? TEXT("true") : TEXT("false"),
                   input.bIsEditable ? TEXT("true") : TEXT("false"),
                   input.bIsStaticSwitch ? TEXT("static") : TEXT("dynamic"));

            if (input.Name == TEXT("Color Mode") || input.Name == TEXT("Sprite Size Mode") ||
                input.Name == TEXT("Shape Primitive")) {
                if (auto const* input_enum{input.Type.GetEnum()}; input_enum != nullptr) {
                    TArray<FString> enum_values{};
                    auto const enum_count{input_enum->NumEnums()};
                    enum_values.Reserve(enum_count);
                    for (int32 enum_index{0}; enum_index < enum_count; ++enum_index) {
                        if (!input_enum->HasMetaData(TEXT("Hidden"), enum_index)) {
                            enum_values.Add(FString::Printf(
                                TEXT("%s=%s"),
                                *input_enum->GetNameStringByIndex(enum_index),
                                *input_enum->GetDisplayNameTextByIndex(enum_index).ToString()));
                        }
                    }
                    UE_LOG(LogSandboxNiagaraInspect,
                           Display,
                           TEXT("        Enum values: %s"),
                           *FString::Join(enum_values, TEXT(", ")));
                }
            }
        }
    }
}
}

USandboxNiagaraInspectCommandlet::USandboxNiagaraInspectCommandlet() {
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
    ShowErrorCount = true;
}

int32 USandboxNiagaraInspectCommandlet::Main(FString const& parameters) {
    FString asset_path{seed_asset_path};
    FParse::Value(*parameters, TEXT("Asset="), asset_path);
    auto* const system{LoadObject<UNiagaraSystem>(nullptr, *asset_path)};
    if (!IsValid(system)) {
        UE_LOG(LogSandboxNiagaraInspect,
               Error,
               TEXT("Failed to load Niagara System: %s"),
               *asset_path);
        return 1;
    }

    system->WaitForCompilationComplete(true, false);

    FNiagaraExternalEditContext context{system};
    FNiagaraExt_SystemSummary summary{};
    UNiagaraExternalEditUtilities::GetSystemSummary(system, summary, context);

    UE_LOG(LogSandboxNiagaraInspect,
           Display,
           TEXT("System %s has %d emitter(s)."),
           *summary.SystemName.ToString(),
           summary.Emitters.Num());

    for (FNiagaraExt_EmitterSummary const& emitter : summary.Emitters) {
        UE_LOG(LogSandboxNiagaraInspect,
               Display,
               TEXT("Emitter %s enabled=%s sim_target=%s renderers=%d"),
               *emitter.EmitterName.ToString(),
               emitter.bEnabled ? TEXT("true") : TEXT("false"),
               emitter.SimTarget == ENiagaraSimTarget::GPUComputeSim ? TEXT("GPU") : TEXT("CPU"),
               emitter.RendererClasses.Num());

        FNiagaraExt_StackItemReference const emitter_ref{system, emitter.EmitterName};
        FNiagaraExt_EmitterTopology topology{};
        UNiagaraExternalEditUtilities::GetEmitterTopology(emitter_ref, topology, context);

        log_stack(topology.EmitterSpawnScript);
        log_stack(topology.EmitterUpdateScript);
        log_stack(topology.ParticleSpawnScript);
        log_stack(topology.ParticleUpdateScript);

        for (FNiagaraExt_RendererRef const& renderer : topology.Renderers) {
            UE_LOG(LogSandboxNiagaraInspect,
                   Display,
                   TEXT("  Renderer[%d] %s"),
                   renderer.RendererIndex,
                   renderer.RendererClass != nullptr
                       ? *renderer.RendererClass->GetPathName()
                       : TEXT("<null>"));
        }
    }

    for (FText const& error : context.Errors) {
        UE_LOG(LogSandboxNiagaraInspect, Error, TEXT("%s"), *error.ToString());
    }

    FNiagaraExt_SystemCompileState compile_state{};
    UNiagaraExternalEditUtilities::GetSystemCompileState(system, compile_state, context);
    if (compile_state.bHasErrors || compile_state.bIsCompiling || compile_state.bIsStale) {
        UE_LOG(LogSandboxNiagaraInspect,
               Error,
               TEXT("The Niagara System is not compile-clean: %s"),
               *asset_path);
        return 1;
    }

    auto const create_control_panel{FParse::Param(*parameters, TEXT("CreateControlPanel"))};
    auto const generate{FParse::Param(*parameters, TEXT("Generate"))};
    auto const regenerate_all{FParse::Param(*parameters, TEXT("RegenerateAll"))};
    if (create_control_panel && (generate || regenerate_all)) {
        UE_LOG(LogSandboxNiagaraInspect,
               Error,
               TEXT("CreateControlPanel must run separately from Niagara generation so Unreal "
                    "can complete the Widget Blueprint save before saving Niagara graphs."));
        return 1;
    }
    if (create_control_panel || generate || regenerate_all) {
        if (GEditor == nullptr) {
            UE_LOG(LogSandboxNiagaraInspect, Error, TEXT("The Unreal Editor is unavailable."));
            return 1;
        }

        auto* const subsystem{GEditor->GetEditorSubsystem<USandboxNiagaraSubsystem>()};
        if (subsystem == nullptr) {
            UE_LOG(LogSandboxNiagaraInspect,
                   Error,
                   TEXT("The Sandbox Niagara subsystem is unavailable."));
            return 1;
        }

        if (create_control_panel && !create_control_panel_asset()) {
            return 1;
        }

        if (regenerate_all) {
            auto const regeneration_result{subsystem->regenerate_all_presets(system)};
            for (FString const& warning : regeneration_result.warnings) {
                UE_LOG(LogSandboxNiagaraInspect, Warning, TEXT("%s"), *warning);
            }
            for (FString const& error : regeneration_result.errors) {
                UE_LOG(LogSandboxNiagaraInspect, Error, TEXT("%s"), *error);
            }
            if (!regeneration_result.success) {
                return 1;
            }
            UE_LOG(LogSandboxNiagaraInspect, Display, TEXT("Regenerated all Niagara presets."));
        }

        if (generate) {
            auto const preset{FParse::Param(*parameters, TEXT("Lorenz"))
                                  ? ESandboxNiagaraExperimentPreset::LorenzAttractor
                                  : ESandboxNiagaraExperimentPreset::Orbit};
            FString experiment_name{subsystem->get_default_experiment_name(preset)};
            FParse::Value(*parameters, TEXT("Name="), experiment_name);
            auto const generation_result{subsystem->generate_preset(
                system,
                preset,
                experiment_name,
                FParse::Param(*parameters, TEXT("Replace")))};
            for (FString const& warning : generation_result.warnings) {
                UE_LOG(LogSandboxNiagaraInspect, Warning, TEXT("%s"), *warning);
            }
            for (FString const& error : generation_result.errors) {
                UE_LOG(LogSandboxNiagaraInspect, Error, TEXT("%s"), *error);
            }
            if (!generation_result.success) {
                return 1;
            }

            UE_LOG(LogSandboxNiagaraInspect,
                   Display,
                   TEXT("Generated configured system: %s"),
                   *generation_result.generated_asset_path);
        }
    }

    return context.HasErrors() ? 1 : 0;
}
