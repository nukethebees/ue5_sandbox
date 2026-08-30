#include "SandboxNiagaraInspectCommandlet.h"

#include "SandboxNiagaraSubsystem.h"

#include "NiagaraExternalSystemEditorUtilities.h"
#include "NiagaraSystem.h"

#include "Editor.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogSandboxNiagaraInspect, Log, All);

namespace {
TCHAR const* const seed_asset_path{
    TEXT("/SandboxNiagara/Templates/NS_SandboxNiagaraSeed.NS_SandboxNiagaraSeed")};

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

    if (FParse::Param(*parameters, TEXT("Generate"))) {
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

        FSandboxNiagaraExperimentConfiguration configuration{};
        FString experiment_name{TEXT("NS_SandboxNiagaraConfigured")};
        if (FParse::Param(*parameters, TEXT("Orbit"))) {
            experiment_name = TEXT("NS_SandboxNiagaraOrbit");
            configuration.spawn_rate = 2000.0f;
            configuration.particle_lifetime = 20.0f;
            configuration.particle_velocity_expression =
                TEXT("float3(-Particles.Position.y, Particles.Position.x, 0.0f) * 0.35f");
        }
        FParse::Value(*parameters, TEXT("Name="), experiment_name);
        auto const generation_result{
            subsystem->generate_experiment(system, experiment_name, configuration)};
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

    return context.HasErrors() ? 1 : 0;
}
