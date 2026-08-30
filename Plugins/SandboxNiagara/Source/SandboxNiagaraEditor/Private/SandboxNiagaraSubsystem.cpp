#include "SandboxNiagaraSubsystem.h"

#include "NiagaraExternalSystemEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"

#include "Editor.h"
#include "Misc/PackageName.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogSandboxNiagara, Log, All);

namespace {
FString const generated_output_root{TEXT("/SandboxNiagara/Generated")};

void add_error(FSandboxNiagaraGenerationResult& result, FString message) {
    UE_LOG(LogSandboxNiagara, Error, TEXT("%s"), *message);
    result.errors.Add(MoveTemp(message));
}

void add_warning(FSandboxNiagaraGenerationResult& result, FString message) {
    UE_LOG(LogSandboxNiagara, Warning, TEXT("%s"), *message);
    result.warnings.Add(MoveTemp(message));
}

void append_context_errors(FNiagaraExternalEditContext const& context,
                           FSandboxNiagaraGenerationResult& result) {
    for (FText const& error : context.Errors) {
        add_error(result, error.ToString());
    }
}

auto compile_status_name(ENiagaraExt_ScriptCompileStatus const status) -> FString {
    auto const* status_enum{StaticEnum<ENiagaraExt_ScriptCompileStatus>()};
    return status_enum != nullptr ? status_enum->GetNameStringByValue(static_cast<int64>(status))
                                  : TEXT("Unknown");
}

auto format_compile_message(FNiagaraExt_ScriptCompileInfo const& script,
                            FString const& message) -> FString {
    auto const owner{script.EmitterName.IsNone() ? TEXT("System")
                                                : script.EmitterName.ToString()};
    auto const script_name{script.ScriptName.IsNone() ? TEXT("UnknownScript")
                                                      : script.ScriptName.ToString()};
    return FString::Printf(TEXT("%s/%s: %s"), *owner, *script_name, *message);
}

auto collect_compile_diagnostics(UNiagaraSystem& system,
                                 FSandboxNiagaraGenerationResult& result) -> bool {
    FNiagaraExternalEditContext context{&system};
    FNiagaraExt_SystemCompileState compile_state{};
    UNiagaraExternalEditUtilities::GetSystemCompileState(&system, compile_state, context);

    append_context_errors(context, result);
    result.compile_status = compile_status_name(compile_state.AggregateStatus);

    for (FNiagaraExt_ScriptCompileInfo const& script : compile_state.Scripts) {
        if (!script.ErrorSummary.IsEmpty()) {
            add_error(result, format_compile_message(script, script.ErrorSummary));
        }

        for (FNiagaraExt_CompileEvent const& event : script.CompileEvents) {
            auto const message{format_compile_message(script, event.Message)};
            if (event.Severity == ENiagaraExt_CompileEventSeverity::Error) {
                add_error(result, message);
            } else if (event.Severity == ENiagaraExt_CompileEventSeverity::Warning) {
                add_warning(result, message);
            }
        }
    }

    if (compile_state.bIsCompiling || compile_state.bIsStale) {
        add_error(result,
                  FString::Printf(TEXT("Niagara compilation did not settle for %s."),
                                  *system.GetPathName()));
    }

    if (compile_state.bHasErrors && result.errors.IsEmpty()) {
        add_error(result,
                  FString::Printf(TEXT("Niagara reported compile errors for %s."),
                                  *system.GetPathName()));
    }

    return !compile_state.bHasErrors && !compile_state.bIsCompiling &&
           !compile_state.bIsStale && !context.HasErrors();
}

auto normalize_output_path(FString output_path) -> FString {
    output_path.TrimStartAndEndInline();
    while (output_path.EndsWith(TEXT("/"))) {
        output_path.LeftChopInline(1);
    }
    return output_path;
}

auto is_generated_output_path(FString const& output_path) -> bool {
    return output_path == generated_output_root ||
           output_path.StartsWith(generated_output_root + TEXT("/"));
}

auto validate_stack(FNiagaraExt_ScriptStackTopology const& stack,
                    TConstArrayView<FName> const expected_modules,
                    FSandboxNiagaraGenerationResult& result) -> bool {
    bool valid{true};
    if (stack.Modules.Num() != expected_modules.Num()) {
        add_error(result,
                  FString::Printf(TEXT("Stack %s must contain exactly %d module(s); found %d."),
                                  *stack.ScriptName.ToString(),
                                  expected_modules.Num(),
                                  stack.Modules.Num()));
        valid = false;
    }

    auto const comparable_count{FMath::Min(stack.Modules.Num(), expected_modules.Num())};
    for (int32 module_index{0}; module_index < comparable_count; ++module_index) {
        auto const actual_name{stack.Modules[module_index].ModuleName};
        auto const expected_name{expected_modules[module_index]};
        if (actual_name != expected_name) {
            add_error(result,
                      FString::Printf(TEXT("Stack %s module %d must be %s; found %s."),
                                      *stack.ScriptName.ToString(),
                                      module_index,
                                      *expected_name.ToString(),
                                      *actual_name.ToString()));
            valid = false;
        }
    }

    return valid;
}

auto validate_template_structure(UNiagaraSystem& system,
                                 FSandboxNiagaraGenerationResult& result) -> bool {
    system.WaitForCompilationComplete(true, false);
    bool valid{collect_compile_diagnostics(system, result)};

    FNiagaraExternalEditContext context{&system};
    FNiagaraExt_SystemSummary summary{};
    UNiagaraExternalEditUtilities::GetSystemSummary(&system, summary, context);
    append_context_errors(context, result);

    if (summary.Emitters.Num() != 1) {
        add_error(result,
                  FString::Printf(TEXT("The template must contain exactly one emitter; found %d."),
                                  summary.Emitters.Num()));
        return false;
    }

    auto const& emitter{summary.Emitters[0]};
    if (emitter.EmitterName != TEXT("SandboxEmitter")) {
        add_error(result,
                  FString::Printf(TEXT("The template emitter must be named SandboxEmitter; found %s."),
                                  *emitter.EmitterName.ToString()));
        valid = false;
    }
    if (!emitter.bEnabled) {
        add_error(result, TEXT("SandboxEmitter must be enabled."));
        valid = false;
    }
    if (emitter.SimTarget != ENiagaraSimTarget::GPUComputeSim) {
        add_warning(result, TEXT("SandboxEmitter is not configured for GPU simulation; generated "
                                 "copies will be switched to GPU simulation."));
    }

    FNiagaraExt_StackItemReference const emitter_ref{&system, emitter.EmitterName};
    FNiagaraExt_EmitterTopology topology{};
    FNiagaraExternalEditContext topology_context{&system};
    UNiagaraExternalEditUtilities::GetEmitterTopology(
        emitter_ref, topology, topology_context);
    append_context_errors(topology_context, result);
    valid &= !topology_context.HasErrors();

    TArray<FName> const no_modules{};
    TArray<FName> const emitter_update_modules{TEXT("EmitterState"), TEXT("SpawnRate")};
    TArray<FName> const particle_spawn_modules{TEXT("InitializeParticle"),
                                               TEXT("ShapeLocation")};
    TArray<FName> const particle_update_modules{TEXT("ParticleState")};
    valid &= validate_stack(topology.EmitterSpawnScript, no_modules, result);
    valid &= validate_stack(topology.EmitterUpdateScript, emitter_update_modules, result);
    valid &= validate_stack(topology.ParticleSpawnScript, particle_spawn_modules, result);
    valid &= validate_stack(topology.ParticleUpdateScript, particle_update_modules, result);

    if (topology.Renderers.Num() != 1) {
        add_error(result,
                  FString::Printf(TEXT("SandboxEmitter must contain exactly one renderer; found %d."),
                                  topology.Renderers.Num()));
        valid = false;
    } else if (topology.Renderers[0].RendererClass.Get() !=
               UNiagaraSpriteRendererProperties::StaticClass()) {
        add_error(result,
                  FString::Printf(TEXT("SandboxEmitter renderer must be a sprite renderer; found %s."),
                                  topology.Renderers[0].RendererClass != nullptr
                                      ? *topology.Renderers[0].RendererClass->GetPathName()
                                      : TEXT("<null>")));
        valid = false;
    }

    return valid && result.errors.IsEmpty();
}

auto set_stack_input(UNiagaraSystem& system,
                     FName const emitter_name,
                     FName const script_name,
                     FName const module_name,
                     FName const input_name,
                     FNiagaraExt_StackInputValue const& value,
                     FSandboxNiagaraGenerationResult& result) -> bool {
    FNiagaraExt_StackItemReference input_ref{&system, emitter_name, script_name, module_name};
    input_ref.InputNameStack.Add(input_name);

    FNiagaraExternalEditContext context{&system};
    UNiagaraExternalEditUtilities::SetStackInputData(input_ref, value, context);
    append_context_errors(context, result);
    return !context.HasErrors();
}

auto set_float_input(UNiagaraSystem& system,
                     FName const emitter_name,
                     FName const script_name,
                     FName const module_name,
                     FName const input_name,
                     float const value,
                     FSandboxNiagaraGenerationResult& result) -> bool {
    FNiagaraExt_StackInputValue input_value{};
    input_value.InitializeAs<FNiagaraFloat>().Value = value;
    return set_stack_input(system,
                           emitter_name,
                           script_name,
                           module_name,
                           input_name,
                           input_value,
                           result);
}

auto set_bool_input(UNiagaraSystem& system,
                    FName const emitter_name,
                    FName const script_name,
                    FName const module_name,
                    FName const input_name,
                    bool const value,
                    FSandboxNiagaraGenerationResult& result) -> bool {
    FNiagaraExt_StackInputValue input_value{};
    input_value.InitializeAs<FNiagaraBool>().SetValue(value);
    return set_stack_input(system,
                           emitter_name,
                           script_name,
                           module_name,
                           input_name,
                           input_value,
                           result);
}

auto set_color_input(UNiagaraSystem& system,
                     FName const emitter_name,
                     FName const script_name,
                     FName const module_name,
                     FName const input_name,
                     FLinearColor const value,
                     FSandboxNiagaraGenerationResult& result) -> bool {
    FNiagaraExt_StackInputValue input_value{};
    input_value.InitializeAs<FLinearColor>() = value;
    return set_stack_input(system,
                           emitter_name,
                           script_name,
                           module_name,
                           input_name,
                           input_value,
                           result);
}

auto set_enum_input(UNiagaraSystem& system,
                    FName const emitter_name,
                    FName const script_name,
                    FName const module_name,
                    FName const input_name,
                    FString const& desired_display_name,
                    FSandboxNiagaraGenerationResult& result) -> bool {
    FNiagaraExt_StackItemReference input_ref{&system, emitter_name, script_name, module_name};
    input_ref.InputNameStack.Add(input_name);

    FNiagaraExternalEditContext context{&system};
    FNiagaraExt_StackInputTopology topology{};
    UNiagaraExternalEditUtilities::GetStackInputTopology(input_ref, topology, context);
    if (context.HasErrors()) {
        append_context_errors(context, result);
        return false;
    }

    auto* const input_enum{topology.Type.GetEnum()};
    if (input_enum == nullptr) {
        add_error(result,
                  FString::Printf(TEXT("Input %s.%s is not an enum."),
                                  *module_name.ToString(),
                                  *input_name.ToString()));
        return false;
    }

    FName enum_name{NAME_None};
    auto const enum_count{input_enum->NumEnums()};
    for (int32 enum_index{0}; enum_index < enum_count; ++enum_index) {
        if (input_enum->GetDisplayNameTextByIndex(enum_index).ToString() == desired_display_name) {
            enum_name = input_enum->GetNameByIndex(enum_index);
            break;
        }
    }

    if (enum_name.IsNone()) {
        add_error(result,
                  FString::Printf(TEXT("Input %s.%s has no enum value displayed as '%s'."),
                                  *module_name.ToString(),
                                  *input_name.ToString(),
                                  *desired_display_name));
        return false;
    }

    FNiagaraExt_StackInputValue input_value{};
    auto& enum_value{input_value.InitializeAs<FNiagaraExt_StackInputData_Enum>()};
    enum_value.Enum = input_enum;
    enum_value.EnumName = enum_name;
    enum_value.DisplayName = FText::FromString(desired_display_name);
    UNiagaraExternalEditUtilities::SetStackInputData(input_ref, input_value, context);
    append_context_errors(context, result);
    return !context.HasErrors();
}

auto get_stack_input(UNiagaraSystem& system,
                     FName const emitter_name,
                     FName const script_name,
                     FName const module_name,
                     FName const input_name,
                     FNiagaraExt_StackInputValue& value,
                     FSandboxNiagaraGenerationResult& result) -> bool {
    FNiagaraExt_StackItemReference input_ref{&system, emitter_name, script_name, module_name};
    input_ref.InputNameStack.Add(input_name);

    FNiagaraExternalEditContext context{&system};
    UNiagaraExternalEditUtilities::GetStackInputData(input_ref, value, context);
    append_context_errors(context, result);
    return !context.HasErrors();
}

auto verify_float_input(UNiagaraSystem& system,
                        FName const emitter_name,
                        FName const script_name,
                        FName const module_name,
                        FName const input_name,
                        float const expected,
                        FSandboxNiagaraGenerationResult& result) -> bool {
    FNiagaraExt_StackInputValue value{};
    if (!get_stack_input(
            system, emitter_name, script_name, module_name, input_name, value, result)) {
        return false;
    }

    auto const* actual{value.GetPtr<FNiagaraFloat>()};
    if (actual == nullptr || !FMath::IsNearlyEqual(actual->Value, expected)) {
        add_error(result,
                  FString::Printf(TEXT("Configured input %s.%s did not retain value %g."),
                                  *module_name.ToString(),
                                  *input_name.ToString(),
                                  expected));
        return false;
    }
    return true;
}

auto verify_bool_input(UNiagaraSystem& system,
                       FName const emitter_name,
                       FName const script_name,
                       FName const module_name,
                       FName const input_name,
                       bool const expected,
                       FSandboxNiagaraGenerationResult& result) -> bool {
    FNiagaraExt_StackInputValue value{};
    if (!get_stack_input(
            system, emitter_name, script_name, module_name, input_name, value, result)) {
        return false;
    }

    auto const* actual{value.GetPtr<FNiagaraBool>()};
    if (actual == nullptr || actual->GetValue() != expected) {
        add_error(result,
                  FString::Printf(TEXT("Configured input %s.%s did not retain value %s."),
                                  *module_name.ToString(),
                                  *input_name.ToString(),
                                  expected ? TEXT("true") : TEXT("false")));
        return false;
    }
    return true;
}

auto verify_color_input(UNiagaraSystem& system,
                        FName const emitter_name,
                        FName const script_name,
                        FName const module_name,
                        FName const input_name,
                        FLinearColor const& expected,
                        FSandboxNiagaraGenerationResult& result) -> bool {
    FNiagaraExt_StackInputValue value{};
    if (!get_stack_input(
            system, emitter_name, script_name, module_name, input_name, value, result)) {
        return false;
    }

    auto const* actual{value.GetPtr<FLinearColor>()};
    if (actual == nullptr || !actual->Equals(expected)) {
        add_error(result,
                  FString::Printf(TEXT("Configured input %s.%s did not retain the expected color."),
                                  *module_name.ToString(),
                                  *input_name.ToString()));
        return false;
    }
    return true;
}

auto verify_enum_input(UNiagaraSystem& system,
                       FName const emitter_name,
                       FName const script_name,
                       FName const module_name,
                       FName const input_name,
                       FString const& expected_display_name,
                       FSandboxNiagaraGenerationResult& result) -> bool {
    FNiagaraExt_StackInputValue value{};
    if (!get_stack_input(
            system, emitter_name, script_name, module_name, input_name, value, result)) {
        return false;
    }

    auto const* actual{value.GetPtr<FNiagaraExt_StackInputData_Enum>()};
    if (actual == nullptr || actual->DisplayName.ToString() != expected_display_name) {
        add_error(result,
                  FString::Printf(TEXT("Configured input %s.%s did not retain enum value '%s'."),
                                  *module_name.ToString(),
                                  *input_name.ToString(),
                                  *expected_display_name));
        return false;
    }
    return true;
}

auto apply_baseline_configuration(UNiagaraSystem& system,
                                  FSandboxNiagaraExperimentConfiguration const& configuration,
                                  FSandboxNiagaraGenerationResult& result) -> bool {
    if (configuration.spawn_rate < 0.0f || configuration.particle_lifetime <= 0.0f ||
        configuration.sprite_size <= 0.0f || configuration.spawn_radius < 0.0f ||
        configuration.fixed_bounds_extent <= 0.0f) {
        add_error(result,
                  TEXT("Spawn rate and radius must be non-negative; lifetime, sprite size, and "
                       "fixed-bounds extent must be positive."));
        return false;
    }

    auto& emitter_handles{system.GetEmitterHandles()};
    if (emitter_handles.Num() != 1) {
        add_error(result, TEXT("Cannot configure a system without exactly one emitter."));
        return false;
    }

    auto& emitter_handle{emitter_handles[0]};
    auto* const emitter{emitter_handle.GetInstance().Emitter.Get()};
    auto* const emitter_data{emitter_handle.GetEmitterData()};
    if (!IsValid(emitter) || emitter_data == nullptr) {
        add_error(result, TEXT("SandboxEmitter or its version data is invalid."));
        return false;
    }

    system.Modify();
    emitter->Modify();
    emitter_handle.SetIsEnabled(true, system, true);
    emitter_data->SimTarget = ENiagaraSimTarget::GPUComputeSim;
    emitter_data->bLocalSpace = false;
    emitter_data->CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Fixed;
    auto const bounds_extent{FVector{configuration.fixed_bounds_extent}};
    emitter_data->FixedBounds = FBox{-bounds_extent, bounds_extent};
    emitter->PostEditChange();
    system.PostEditChange();

    auto const emitter_name{emitter_handle.GetName()};
    bool success{true};
    success &= set_float_input(system,
                               emitter_name,
                               TEXT("EmitterUpdateScript"),
                               TEXT("SpawnRate"),
                               TEXT("SpawnRate"),
                               configuration.spawn_rate,
                               result);
    success &= set_enum_input(system,
                              emitter_name,
                              TEXT("ParticleSpawnScript"),
                              TEXT("InitializeParticle"),
                              TEXT("Color Mode"),
                              TEXT("Direct Set"),
                              result);
    success &= set_color_input(system,
                               emitter_name,
                               TEXT("ParticleSpawnScript"),
                               TEXT("InitializeParticle"),
                               TEXT("Color"),
                               FLinearColor::White,
                               result);
    success &= set_float_input(system,
                               emitter_name,
                               TEXT("ParticleSpawnScript"),
                               TEXT("InitializeParticle"),
                               TEXT("Lifetime"),
                               configuration.particle_lifetime,
                               result);
    success &= set_enum_input(system,
                              emitter_name,
                              TEXT("ParticleSpawnScript"),
                              TEXT("InitializeParticle"),
                              TEXT("Sprite Size Mode"),
                              TEXT("Uniform"),
                              result);
    success &= set_float_input(system,
                               emitter_name,
                               TEXT("ParticleSpawnScript"),
                               TEXT("InitializeParticle"),
                               TEXT("Uniform Sprite Size"),
                               configuration.sprite_size,
                               result);
    success &= set_enum_input(system,
                              emitter_name,
                              TEXT("ParticleSpawnScript"),
                              TEXT("ShapeLocation"),
                              TEXT("Shape Primitive"),
                              TEXT("Sphere"),
                              result);
    success &= set_float_input(system,
                               emitter_name,
                               TEXT("ParticleSpawnScript"),
                               TEXT("ShapeLocation"),
                               TEXT("Sphere Radius"),
                               configuration.spawn_radius,
                               result);
    success &= set_bool_input(system,
                              emitter_name,
                              TEXT("ParticleUpdateScript"),
                              TEXT("ParticleState"),
                              TEXT("Kill Particles When Lifetime Has Elapsed"),
                              true,
                              result);

    return success;
}

auto verify_baseline_configuration(UNiagaraSystem& system,
                                   FSandboxNiagaraExperimentConfiguration const& configuration,
                                   FSandboxNiagaraGenerationResult& result) -> bool {
    auto const& emitter_handles{system.GetEmitterHandles()};
    if (emitter_handles.Num() != 1) {
        add_error(result, TEXT("Generated system no longer contains exactly one emitter."));
        return false;
    }

    auto const& emitter_handle{emitter_handles[0]};
    auto const* emitter_data{emitter_handle.GetEmitterData()};
    if (emitter_data == nullptr) {
        add_error(result, TEXT("Generated SandboxEmitter version data is invalid."));
        return false;
    }

    bool success{true};
    if (!emitter_handle.GetIsEnabled()) {
        add_error(result, TEXT("Generated SandboxEmitter is disabled."));
        success = false;
    }
    if (emitter_data->SimTarget != ENiagaraSimTarget::GPUComputeSim) {
        add_error(result, TEXT("Generated SandboxEmitter is not using GPU simulation."));
        success = false;
    }
    if (emitter_data->bLocalSpace) {
        add_error(result, TEXT("Generated SandboxEmitter unexpectedly uses local space."));
        success = false;
    }
    auto const bounds_extent{FVector{configuration.fixed_bounds_extent}};
    auto const expected_bounds{FBox{-bounds_extent, bounds_extent}};
    if (emitter_data->CalculateBoundsMode != ENiagaraEmitterCalculateBoundMode::Fixed ||
        !emitter_data->FixedBounds.Equals(expected_bounds)) {
        add_error(result, TEXT("Generated SandboxEmitter does not have the configured fixed bounds."));
        success = false;
    }

    auto const emitter_name{emitter_handle.GetName()};
    success &= verify_float_input(system,
                                  emitter_name,
                                  TEXT("EmitterUpdateScript"),
                                  TEXT("SpawnRate"),
                                  TEXT("SpawnRate"),
                                  configuration.spawn_rate,
                                  result);
    success &= verify_enum_input(system,
                                 emitter_name,
                                 TEXT("ParticleSpawnScript"),
                                 TEXT("InitializeParticle"),
                                 TEXT("Color Mode"),
                                 TEXT("Direct Set"),
                                 result);
    success &= verify_color_input(system,
                                  emitter_name,
                                  TEXT("ParticleSpawnScript"),
                                  TEXT("InitializeParticle"),
                                  TEXT("Color"),
                                  FLinearColor::White,
                                  result);
    success &= verify_float_input(system,
                                  emitter_name,
                                  TEXT("ParticleSpawnScript"),
                                  TEXT("InitializeParticle"),
                                  TEXT("Lifetime"),
                                  configuration.particle_lifetime,
                                  result);
    success &= verify_enum_input(system,
                                 emitter_name,
                                 TEXT("ParticleSpawnScript"),
                                 TEXT("InitializeParticle"),
                                 TEXT("Sprite Size Mode"),
                                 TEXT("Uniform"),
                                 result);
    success &= verify_float_input(system,
                                  emitter_name,
                                  TEXT("ParticleSpawnScript"),
                                  TEXT("InitializeParticle"),
                                  TEXT("Uniform Sprite Size"),
                                  configuration.sprite_size,
                                  result);
    success &= verify_enum_input(system,
                                 emitter_name,
                                 TEXT("ParticleSpawnScript"),
                                 TEXT("ShapeLocation"),
                                 TEXT("Shape Primitive"),
                                 TEXT("Sphere"),
                                 result);
    success &= verify_float_input(system,
                                  emitter_name,
                                  TEXT("ParticleSpawnScript"),
                                  TEXT("ShapeLocation"),
                                  TEXT("Sphere Radius"),
                                  configuration.spawn_radius,
                                  result);
    success &= verify_bool_input(system,
                                 emitter_name,
                                 TEXT("ParticleUpdateScript"),
                                 TEXT("ParticleState"),
                                 TEXT("Kill Particles When Lifetime Has Elapsed"),
                                 true,
                                 result);

    return success;
}
}

FSandboxNiagaraValidationResult
USandboxNiagaraSubsystem::validate_template(UNiagaraSystem* template_system) {
    FSandboxNiagaraValidationResult validation_result{};
    if (!IsValid(template_system)) {
        validation_result.errors.Add(TEXT("A valid Niagara System template is required."));
        return validation_result;
    }

    FSandboxNiagaraGenerationResult internal_result{};
    validation_result.success = validate_template_structure(*template_system, internal_result);
    validation_result.warnings = MoveTemp(internal_result.warnings);
    validation_result.errors = MoveTemp(internal_result.errors);
    return validation_result;
}

FSandboxNiagaraGenerationResult USandboxNiagaraSubsystem::generate_experiment(
    UNiagaraSystem* template_system,
    FString const& experiment_name,
    FSandboxNiagaraExperimentConfiguration const& configuration) {
    FSandboxNiagaraGenerationResult result{};

    if (!IsValid(template_system)) {
        add_error(result, TEXT("A valid Niagara System template is required."));
        return result;
    }

    if (experiment_name.IsEmpty() || experiment_name.Contains(TEXT("/")) ||
        experiment_name.Contains(TEXT("\\"))) {
        add_error(result,
                  FString::Printf(TEXT("Invalid experiment asset name '%s'."), *experiment_name));
        return result;
    }

    auto const output_path{normalize_output_path(configuration.output_path)};
    if (!is_generated_output_path(output_path)) {
        add_error(result,
                  FString::Printf(TEXT("Generated Niagara output must be in %s or one of its "
                                       "subdirectories. Received '%s'."),
                                  *generated_output_root,
                                  *output_path));
        return result;
    }

    FText path_error{};
    if (!FPackageName::IsValidLongPackageName(output_path, true, &path_error)) {
        add_error(result,
                  FString::Printf(TEXT("Invalid generated output path '%s': %s"),
                                  *output_path,
                                  *path_error.ToString()));
        return result;
    }

    auto const destination_package_path{output_path / experiment_name};
    auto const destination_object_path{
        FString::Printf(TEXT("%s.%s"), *destination_package_path, *experiment_name)};
    FText object_path_error{};
    if (!FPackageName::IsValidObjectPath(destination_object_path, &object_path_error)) {
        add_error(result,
                  FString::Printf(TEXT("Invalid generated asset path '%s': %s"),
                                  *destination_object_path,
                                  *object_path_error.ToString()));
        return result;
    }

    if (GEditor == nullptr) {
        add_error(result, TEXT("The Unreal Editor is unavailable."));
        return result;
    }

    auto* const asset_subsystem{GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()};
    if (asset_subsystem == nullptr) {
        add_error(result, TEXT("The Editor Asset Subsystem is unavailable."));
        return result;
    }

    if (asset_subsystem->DoesAssetExist(destination_object_path) ||
        FindPackage(nullptr, *destination_package_path) != nullptr) {
        add_error(result,
                  FString::Printf(TEXT("Generated asset already exists: %s"),
                                  *destination_object_path));
        return result;
    }

    FSandboxNiagaraGenerationResult template_diagnostics{};
    if (!validate_template_structure(*template_system, template_diagnostics)) {
        add_error(result,
                  FString::Printf(TEXT("Template Niagara System is invalid: %s"),
                                  *template_system->GetPathName()));
        result.errors.Append(template_diagnostics.errors);
        result.warnings.Append(template_diagnostics.warnings);
        return result;
    }

    UE_LOG(LogSandboxNiagara,
           Display,
           TEXT("Generating Niagara experiment %s from %s."),
           *destination_object_path,
           *template_system->GetPathName());

    FNiagaraExternalEditContext creation_context{};
    auto* const generated_system{UNiagaraExternalEditUtilities::CreateNiagaraSystem(
        experiment_name, output_path, template_system, creation_context)};
    append_context_errors(creation_context, result);
    if (!IsValid(generated_system)) {
        add_error(result,
                  FString::Printf(TEXT("Failed to create generated Niagara System: %s"),
                                  *destination_object_path));
        return result;
    }

    result.generated_system = generated_system;
    result.generated_asset_path = generated_system->GetPathName();

    if (!apply_baseline_configuration(*generated_system, configuration, result)) {
        add_error(result,
                  FString::Printf(TEXT("Failed to configure generated Niagara System: %s"),
                                  *generated_system->GetPathName()));
    }

    generated_system->RequestCompile(true);
    generated_system->WaitForCompilationComplete(true, false);
    auto const compile_success{collect_compile_diagnostics(*generated_system, result)};
    auto const configuration_verified{
        verify_baseline_configuration(*generated_system, configuration, result)};

    auto const save_success{asset_subsystem->SaveLoadedAsset(generated_system, false)};
    if (!save_success) {
        add_error(result,
                  FString::Printf(TEXT("Failed to save generated Niagara System: %s"),
                                  *generated_system->GetPathName()));
    }

    result.success = compile_success && configuration_verified && save_success &&
                     result.errors.IsEmpty();
    if (result.success) {
        UE_LOG(LogSandboxNiagara,
               Display,
               TEXT("Generated Niagara experiment %s with compile status %s."),
               *generated_system->GetPathName(),
               *result.compile_status);
    }

    return result;
}
