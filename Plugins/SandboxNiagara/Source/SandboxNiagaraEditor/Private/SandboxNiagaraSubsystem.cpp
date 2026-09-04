#include "SandboxNiagaraSubsystem.h"
#include "SandboxNiagaraShowcaseActor.h"

#include "NiagaraExternalSystemEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemEditorData.h"

#include "Camera/CameraActor.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "LevelEditorSubsystem.h"
#include "LevelEditorViewport.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogSandboxNiagara, Log, All);

namespace {
FString const generated_output_root{TEXT("/SandboxNiagara/Generated")};
FString const showcase_package_path{
    TEXT("/SandboxNiagara/Generated/Showcase/SandboxNiagara_Showcase")};
FString const showcase_object_path{
    TEXT("/SandboxNiagara/Generated/Showcase/SandboxNiagara_Showcase.SandboxNiagara_Showcase")};
FName const showcase_camera_tag{TEXT("SandboxNiagaraShowcaseCamera")};
constexpr int32 showcase_column_count{3};
constexpr float showcase_cell_padding{500.0f};
constexpr float showcase_label_padding{150.0f};
constexpr float showcase_burst_scale{3.0f};
constexpr float showcase_replay_interval{1.25f};
constexpr float burst_preview_tail{0.5f};

struct FShowcaseLayout {
    TArray<FVector> effect_locations{};
    FVector camera_location{};
    FRotator camera_rotation{};
};

void add_validation_error(FSandboxNiagaraValidationResult& result, FString message) {
    UE_LOG(LogSandboxNiagara, Error, TEXT("%s"), *message);
    result.errors.Add(MoveTemp(message));
}

auto build_showcase_layout(
    TArray<FSandboxNiagaraExperimentDefinition> const& experiments) -> FShowcaseLayout {
    FShowcaseLayout layout{};
    auto const experiment_count{experiments.Num()};
    if (experiment_count == 0) {
        return layout;
    }

    auto const column_count{FMath::Min(showcase_column_count, experiment_count)};
    auto const row_count{FMath::DivideAndRoundUp(experiment_count, column_count)};
    TArray<float> column_extents{};
    TArray<float> row_extents{};
    column_extents.Init(0.0f, column_count);
    row_extents.Init(0.0f, row_count);

    for (int32 experiment_index{0}; experiment_index < experiment_count; ++experiment_index) {
        auto const column_index{experiment_index % column_count};
        auto const row_index{experiment_index / column_count};
        auto const& configuration{experiments[experiment_index].configuration};
        auto const scale{configuration.emission_mode == ESandboxNiagaraEmissionMode::Burst
                             ? showcase_burst_scale
                             : 1.0f};
        auto const extent{configuration.fixed_bounds_extent * scale};
        column_extents[column_index] = FMath::Max(column_extents[column_index], extent);
        row_extents[row_index] = FMath::Max(row_extents[row_index], extent);
    }

    TArray<float> column_locations{};
    TArray<float> row_locations{};
    column_locations.SetNum(column_count);
    row_locations.SetNum(row_count);
    auto column_cursor{0.0f};
    for (int32 column_index{0}; column_index < column_count; ++column_index) {
        column_locations[column_index] = column_cursor + column_extents[column_index];
        column_cursor += column_extents[column_index] * 2.0f + showcase_cell_padding;
    }
    auto const layout_width{column_cursor - showcase_cell_padding};
    for (float& location : column_locations) {
        location -= layout_width * 0.5f;
    }

    auto row_cursor{0.0f};
    for (int32 row_index{0}; row_index < row_count; ++row_index) {
        row_locations[row_index] = row_cursor + row_extents[row_index];
        row_cursor += row_extents[row_index] * 2.0f + showcase_cell_padding;
    }
    auto const layout_depth{row_cursor - showcase_cell_padding};
    for (float& location : row_locations) {
        location -= layout_depth * 0.5f;
    }

    layout.effect_locations.Reserve(experiment_count);
    for (int32 experiment_index{0}; experiment_index < experiment_count; ++experiment_index) {
        auto const column_index{experiment_index % column_count};
        auto const row_index{experiment_index / column_count};
        layout.effect_locations.Add(
            FVector{column_locations[column_index], row_locations[row_index], 0.0f});
    }

    auto const camera_distance{FMath::Max(layout_width, layout_depth) * 1.2f};
    layout.camera_location = FVector{-camera_distance, -camera_distance, camera_distance * 0.7f};
    auto const camera_target{FVector{0.0f, 0.0f, 250.0f}};
    layout.camera_rotation = (camera_target - layout.camera_location).Rotation();
    return layout;
}

void frame_showcase_camera(UWorld& world, FSandboxNiagaraValidationResult& result) {
    ACameraActor* showcase_camera{nullptr};
    for (TActorIterator<ACameraActor> camera_iterator{&world}; camera_iterator; ++camera_iterator) {
        if (camera_iterator->ActorHasTag(showcase_camera_tag)) {
            showcase_camera = *camera_iterator;
            break;
        }
    }
    if (!IsValid(showcase_camera)) {
        add_validation_error(result, TEXT("The showcase overview camera is missing."));
        return;
    }
    if (GCurrentLevelEditingViewportClient == nullptr) {
        result.warnings.Add(TEXT("The showcase opened, but no level viewport was available to frame it."));
        return;
    }

    GCurrentLevelEditingViewportClient->SetViewLocation(showcase_camera->GetActorLocation());
    GCurrentLevelEditingViewportClient->SetViewRotation(showcase_camera->GetActorRotation());
    GCurrentLevelEditingViewportClient->ViewFOV = 70.0f;
    GCurrentLevelEditingViewportClient->Invalidate();
}

void add_catalog_error(FSandboxNiagaraExperimentCatalogResult& result,
                       FString const& source_file,
                       FString message) {
    message = FString::Printf(TEXT("%s: %s"), *source_file, *message);
    UE_LOG(LogSandboxNiagara, Error, TEXT("%s"), *message);
    result.errors.Add(MoveTemp(message));
}

auto read_required_string(FJsonObject const& object,
                          FStringView const field,
                          FString const& source_file,
                          FString& value,
                          FSandboxNiagaraExperimentCatalogResult& result) -> bool {
    if (!object.TryGetStringField(field, value) || value.TrimStartAndEnd().IsEmpty()) {
        add_catalog_error(result,
                          source_file,
                          FString::Printf(TEXT("Required string field '%.*s' is missing or empty."),
                                          field.Len(),
                                          field.GetData()));
        return false;
    }
    value.TrimStartAndEndInline();
    return true;
}

auto read_required_float(FJsonObject const& object,
                         FStringView const field,
                         FString const& source_file,
                         float& value,
                         FSandboxNiagaraExperimentCatalogResult& result) -> bool {
    if (!object.TryGetNumberField(field, value) || !FMath::IsFinite(value)) {
        add_catalog_error(result,
                          source_file,
                          FString::Printf(TEXT("Required numeric field '%.*s' is missing or invalid."),
                                          field.Len(),
                                          field.GetData()));
        return false;
    }
    return true;
}

auto read_required_int(FJsonObject const& object,
                       FStringView const field,
                       FString const& source_file,
                       int32& value,
                       FSandboxNiagaraExperimentCatalogResult& result) -> bool {
    if (!object.TryGetNumberField(field, value)) {
        add_catalog_error(result,
                          source_file,
                          FString::Printf(TEXT("Required integer field '%.*s' is missing or invalid."),
                                          field.Len(),
                                          field.GetData()));
        return false;
    }
    return true;
}

auto read_required_bool(FJsonObject const& object,
                        FStringView const field,
                        FString const& source_file,
                        bool& value,
                        FSandboxNiagaraExperimentCatalogResult& result) -> bool {
    if (!object.TryGetBoolField(field, value)) {
        add_catalog_error(result,
                          source_file,
                          FString::Printf(TEXT("Required Boolean field '%.*s' is missing or invalid."),
                                          field.Len(),
                                          field.GetData()));
        return false;
    }
    return true;
}

auto read_emission_mode(FJsonObject const& object,
                        FString const& source_file,
                        ESandboxNiagaraEmissionMode& emission_mode,
                        FSandboxNiagaraExperimentCatalogResult& result) -> bool {
    FString value{};
    if (!read_required_string(object, TEXTVIEW("emission_mode"), source_file, value, result)) {
        return false;
    }

    if (value == TEXT("continuous")) {
        emission_mode = ESandboxNiagaraEmissionMode::Continuous;
    } else if (value == TEXT("burst")) {
        emission_mode = ESandboxNiagaraEmissionMode::Burst;
    } else {
        add_catalog_error(
            result,
            source_file,
            FString::Printf(TEXT("Unknown emission mode '%s'."), *value));
        return false;
    }
    return true;
}

auto read_spawn_shape(FJsonObject const& object,
                      FString const& source_file,
                      ESandboxNiagaraSpawnShape& spawn_shape,
                      FSandboxNiagaraExperimentCatalogResult& result) -> bool {
    FString value{};
    if (!read_required_string(object, TEXTVIEW("spawn_shape"), source_file, value, result)) {
        return false;
    }

    if (value == TEXT("sphere")) {
        spawn_shape = ESandboxNiagaraSpawnShape::Sphere;
    } else if (value == TEXT("disc")) {
        spawn_shape = ESandboxNiagaraSpawnShape::Disc;
    } else if (value == TEXT("cylinder")) {
        spawn_shape = ESandboxNiagaraSpawnShape::Cylinder;
    } else if (value == TEXT("ring")) {
        spawn_shape = ESandboxNiagaraSpawnShape::Ring;
    } else {
        add_catalog_error(
            result,
            source_file,
            FString::Printf(TEXT("Unknown spawn shape '%s'."), *value));
        return false;
    }
    return true;
}

auto read_color(FJsonObject const& object,
                FString const& source_file,
                FLinearColor& color,
                FSandboxNiagaraExperimentCatalogResult& result) -> bool {
    TArray<TSharedPtr<FJsonValue>> const* values{nullptr};
    if (!object.TryGetArrayField(TEXTVIEW("particle_color"), values) || values == nullptr ||
        values->Num() != 4) {
        add_catalog_error(result,
                          source_file,
                          TEXT("'particle_color' must contain four numeric RGBA values."));
        return false;
    }

    float channels[4]{};
    for (int32 channel_index{0}; channel_index < 4; ++channel_index) {
        if (!(*values)[channel_index].IsValid() ||
            !(*values)[channel_index]->TryGetNumber(channels[channel_index]) ||
            !FMath::IsFinite(channels[channel_index])) {
            add_catalog_error(result,
                              source_file,
                              TEXT("'particle_color' must contain four finite numeric values."));
            return false;
        }
    }
    color = FLinearColor{channels[0], channels[1], channels[2], channels[3]};
    return true;
}

auto read_parameters(FJsonObject const& object,
                     FString const& source_file,
                     TArray<FSandboxNiagaraFloatParameter>& parameters,
                     FSandboxNiagaraExperimentCatalogResult& result) -> bool {
    TArray<TSharedPtr<FJsonValue>> const* values{nullptr};
    if (!object.TryGetArrayField(TEXTVIEW("parameters"), values) || values == nullptr) {
        add_catalog_error(result, source_file, TEXT("'parameters' must be an array."));
        return false;
    }

    bool valid{true};
    TSet<FName> names{};
    for (int32 parameter_index{0}; parameter_index < values->Num(); ++parameter_index) {
        TSharedPtr<FJsonObject> const* parameter_object{nullptr};
        if (!(*values)[parameter_index].IsValid() ||
            !(*values)[parameter_index]->TryGetObject(parameter_object) ||
            parameter_object == nullptr || !parameter_object->IsValid()) {
            add_catalog_error(result,
                              source_file,
                              FString::Printf(TEXT("Parameter %d must be an object."),
                                              parameter_index));
            valid = false;
            continue;
        }

        FSandboxNiagaraFloatParameter parameter{};
        FString parameter_name{};
        valid &= read_required_string(**parameter_object,
                                      TEXTVIEW("name"),
                                      source_file,
                                      parameter_name,
                                      result);
        valid &= read_required_string(**parameter_object,
                                      TEXTVIEW("display_name"),
                                      source_file,
                                      parameter.display_name,
                                      result);
        valid &= read_required_float(**parameter_object,
                                     TEXTVIEW("default"),
                                     source_file,
                                     parameter.value,
                                     result);
        (*parameter_object)->TryGetNumberField(TEXTVIEW("minimum"), parameter.minimum);
        (*parameter_object)->TryGetNumberField(TEXTVIEW("maximum"), parameter.maximum);
        parameter.name = FName{parameter_name};

        if (!parameter_name.StartsWith(TEXT("User.")) || parameter.name.IsNone()) {
            add_catalog_error(result,
                              source_file,
                              FString::Printf(TEXT("Parameter '%s' must use the User namespace."),
                                              *parameter_name));
            valid = false;
        } else if (names.Contains(parameter.name)) {
            add_catalog_error(result,
                              source_file,
                              FString::Printf(TEXT("Parameter '%s' is defined more than once."),
                                              *parameter_name));
            valid = false;
        } else {
            names.Add(parameter.name);
        }
        if (!FMath::IsFinite(parameter.minimum) || !FMath::IsFinite(parameter.maximum) ||
            parameter.minimum > parameter.maximum || parameter.value < parameter.minimum ||
            parameter.value > parameter.maximum) {
            add_catalog_error(result,
                              source_file,
                              FString::Printf(TEXT("Parameter '%s' has invalid bounds or default."),
                                              *parameter_name));
            valid = false;
        }
        parameters.Add(MoveTemp(parameter));
    }
    return valid;
}

auto read_hlsl_expression(FJsonObject const& object,
                          FString const& experiments_directory,
                          FString const& source_file,
                          FString& expression,
                          FSandboxNiagaraExperimentCatalogResult& result) -> bool {
    FString relative_path{};
    if (!read_required_string(object,
                              TEXTVIEW("velocity_expression_file"),
                              source_file,
                              relative_path,
                              result)) {
        return false;
    }
    if (!FPaths::IsRelative(relative_path)) {
        add_catalog_error(result,
                          source_file,
                          TEXT("'velocity_expression_file' must be relative to Experiments."));
        return false;
    }

    auto expression_path{FPaths::ConvertRelativePathToFull(
        FPaths::Combine(experiments_directory, relative_path))};
    FPaths::CollapseRelativeDirectories(expression_path);
    if (!FPaths::IsUnderDirectory(expression_path, experiments_directory)) {
        add_catalog_error(result,
                          source_file,
                          TEXT("'velocity_expression_file' must stay inside Experiments."));
        return false;
    }
    if (!FFileHelper::LoadFileToString(expression, *expression_path)) {
        add_catalog_error(result,
                          source_file,
                          FString::Printf(TEXT("Could not read HLSL expression file '%s'."),
                                          *relative_path));
        return false;
    }
    expression.TrimStartAndEndInline();
    if (expression.IsEmpty() || expression.Contains(TEXT(";"))) {
        add_catalog_error(result,
                          source_file,
                          TEXT("HLSL expression must be a non-empty single rvalue without ';'."));
        return false;
    }
    return true;
}

auto load_experiment_definition(FString const& filename,
                                FString const& experiments_directory,
                                FSandboxNiagaraExperimentDefinition& definition,
                                FSandboxNiagaraExperimentCatalogResult& result) -> bool {
    FString json_text{};
    if (!FFileHelper::LoadFileToString(json_text, *filename)) {
        add_catalog_error(result, filename, TEXT("Could not read experiment definition."));
        return false;
    }

    TSharedPtr<FJsonObject> root{};
    auto const reader{TJsonReaderFactory<>::Create(json_text)};
    if (!FJsonSerializer::Deserialize(reader, root) || !root.IsValid()) {
        add_catalog_error(result, filename, TEXT("Invalid JSON."));
        return false;
    }

    definition.source_file = filename;
    bool valid{true};
    int32 schema_version{0};
    if (!root->TryGetNumberField(TEXTVIEW("schema_version"), schema_version) ||
        schema_version != 3) {
        add_catalog_error(result, filename, TEXT("'schema_version' must be 3."));
        valid = false;
    }
    valid &= read_required_string(
        *root, TEXTVIEW("id"), filename, definition.id, result);
    valid &= read_required_string(
        *root, TEXTVIEW("display_name"), filename, definition.display_name, result);
    valid &= read_required_string(*root,
                                  TEXTVIEW("default_asset_name"),
                                  filename,
                                  definition.default_asset_name,
                                  result);

    TSharedPtr<FJsonObject> const* configuration_object{nullptr};
    if (!root->TryGetObjectField(TEXTVIEW("configuration"), configuration_object) ||
        configuration_object == nullptr || !configuration_object->IsValid()) {
        add_catalog_error(result, filename, TEXT("'configuration' must be an object."));
        return false;
    }
    auto const& configuration{**configuration_object};
    valid &= read_emission_mode(
        configuration, filename, definition.configuration.emission_mode, result);
    if (definition.configuration.emission_mode == ESandboxNiagaraEmissionMode::Continuous) {
        valid &= read_required_float(configuration,
                                     TEXTVIEW("spawn_rate"),
                                     filename,
                                     definition.configuration.spawn_rate,
                                     result);
    } else {
        valid &= read_required_int(configuration,
                                   TEXTVIEW("burst_count"),
                                   filename,
                                   definition.configuration.burst_count,
                                   result);
    }
    valid &= read_required_bool(configuration,
                                TEXTVIEW("local_space"),
                                filename,
                                definition.configuration.local_space,
                                result);
    valid &= read_required_float(configuration,
                                 TEXTVIEW("particle_lifetime"),
                                 filename,
                                 definition.configuration.particle_lifetime,
                                 result);
    valid &= read_required_float(configuration,
                                 TEXTVIEW("sprite_size"),
                                 filename,
                                 definition.configuration.sprite_size,
                                 result);
    valid &= read_spawn_shape(
        configuration, filename, definition.configuration.spawn_shape, result);
    valid &= read_required_float(configuration,
                                 TEXTVIEW("spawn_radius"),
                                 filename,
                                 definition.configuration.spawn_radius,
                                 result);
    if (definition.configuration.spawn_shape == ESandboxNiagaraSpawnShape::Cylinder) {
        valid &= read_required_float(configuration,
                                     TEXTVIEW("spawn_height"),
                                     filename,
                                     definition.configuration.spawn_height,
                                     result);
    }
    if (definition.configuration.spawn_shape == ESandboxNiagaraSpawnShape::Ring) {
        valid &= read_required_float(configuration,
                                     TEXTVIEW("spawn_inner_radius"),
                                     filename,
                                     definition.configuration.spawn_inner_radius,
                                     result);
    }
    valid &= read_required_float(configuration,
                                 TEXTVIEW("fixed_bounds_extent"),
                                 filename,
                                 definition.configuration.fixed_bounds_extent,
                                 result);
    valid &= read_color(configuration, filename, definition.configuration.particle_color, result);
    valid &= read_parameters(
        configuration, filename, definition.configuration.float_parameters, result);
    valid &= read_hlsl_expression(configuration,
                                  experiments_directory,
                                  filename,
                                  definition.configuration.particle_velocity_expression,
                                  result);
    return valid;
}

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

auto set_int_input(UNiagaraSystem& system,
                   FName const emitter_name,
                   FName const script_name,
                   FName const module_name,
                   FName const input_name,
                   int32 const value,
                   FSandboxNiagaraGenerationResult& result) -> bool {
    FNiagaraExt_StackInputValue input_value{};
    input_value.InitializeAs<FNiagaraInt32>().Value = value;
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

struct FResolvedEnumValue {
    FName name{NAME_None};
    FText display_name{};
};

auto resolve_enum_value(UEnum const& input_enum,
                        FString const& desired_name,
                        FResolvedEnumValue& resolved_value) -> bool {
    auto const enum_count{input_enum.NumEnums()};
    for (int32 enum_index{0}; enum_index < enum_count; ++enum_index) {
        auto const display_name{input_enum.GetDisplayNameTextByIndex(enum_index)};
        auto const display_name_string{display_name.ToString()};
        auto const enum_name_string{input_enum.GetNameStringByIndex(enum_index)};
        auto const display_name_prefix{desired_name + TEXT(" ")};
        if (!display_name_string.Equals(desired_name, ESearchCase::IgnoreCase) &&
            !enum_name_string.Equals(desired_name, ESearchCase::IgnoreCase) &&
            !display_name_string.StartsWith(display_name_prefix, ESearchCase::IgnoreCase)) {
            continue;
        }

        resolved_value.name = input_enum.GetNameByIndex(enum_index);
        resolved_value.display_name = display_name;
        return true;
    }

    return false;
}

auto available_enum_values(UEnum const& input_enum) -> FString {
    TArray<FString> display_names{};
    auto const enum_count{input_enum.NumEnums()};
    display_names.Reserve(enum_count);
    for (int32 enum_index{0}; enum_index < enum_count; ++enum_index) {
        display_names.Add(input_enum.GetDisplayNameTextByIndex(enum_index).ToString());
    }
    return FString::Join(display_names, TEXT(", "));
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

    FResolvedEnumValue resolved_value{};
    if (!resolve_enum_value(*input_enum, desired_display_name, resolved_value)) {
        add_error(result,
                  FString::Printf(TEXT("Input %s.%s has no enum value matching '%s'. Available "
                                       "values: %s."),
                                  *module_name.ToString(),
                                  *input_name.ToString(),
                                  *desired_display_name,
                                  *available_enum_values(*input_enum)));
        return false;
    }

    FNiagaraExt_StackInputValue input_value{};
    auto& enum_value{input_value.InitializeAs<FNiagaraExt_StackInputData_Enum>()};
    enum_value.Enum = input_enum;
    enum_value.EnumName = resolved_value.name;
    enum_value.DisplayName = resolved_value.display_name;
    UNiagaraExternalEditUtilities::SetStackInputData(input_ref, input_value, context);
    append_context_errors(context, result);
    return !context.HasErrors();
}

auto set_hlsl_input(UNiagaraSystem& system,
                    FName const emitter_name,
                    FName const script_name,
                    FName const module_name,
                    FName const input_name,
                    FString const& expression,
                    FSandboxNiagaraGenerationResult& result) -> bool {
    FNiagaraExt_StackInputValue input_value{};
    input_value.InitializeAs<FNiagaraExt_StackInputData_HlslExpression>().HlslExpression =
        expression;
    return set_stack_input(system,
                           emitter_name,
                           script_name,
                           module_name,
                           input_name,
                           input_value,
                           result);
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
    FResolvedEnumValue expected_value{};
    if (actual == nullptr || actual->Enum == nullptr ||
        !resolve_enum_value(*actual->Enum, expected_display_name, expected_value) ||
        actual->EnumName != expected_value.name) {
        add_error(result,
                  FString::Printf(TEXT("Configured input %s.%s did not retain enum value '%s'."),
                                  *module_name.ToString(),
                                  *input_name.ToString(),
                                  *expected_display_name));
        return false;
    }
    return true;
}

auto verify_int_input(UNiagaraSystem& system,
                      FName const emitter_name,
                      FName const script_name,
                      FName const module_name,
                      FName const input_name,
                      int32 const expected,
                      FSandboxNiagaraGenerationResult& result) -> bool {
    FNiagaraExt_StackInputValue value{};
    if (!get_stack_input(
            system, emitter_name, script_name, module_name, input_name, value, result)) {
        return false;
    }

    auto const* actual{value.GetPtr<FNiagaraInt32>()};
    if (actual == nullptr || actual->Value != expected) {
        add_error(result,
                  FString::Printf(TEXT("Configured input %s.%s did not retain value %d."),
                                  *module_name.ToString(),
                                  *input_name.ToString(),
                                  expected));
        return false;
    }
    return true;
}

auto spawn_primitive_display_name(ESandboxNiagaraSpawnShape const spawn_shape) -> FString {
    switch (spawn_shape) {
        case ESandboxNiagaraSpawnShape::Sphere:
            return TEXT("Sphere");
        case ESandboxNiagaraSpawnShape::Disc:
        case ESandboxNiagaraSpawnShape::Ring:
            return TEXT("Ring / Disc");
        case ESandboxNiagaraSpawnShape::Cylinder:
            return TEXT("Cylinder");
    }
    return {};
}

auto ring_disc_coverage(FSandboxNiagaraExperimentConfiguration const& configuration) -> float {
    if (configuration.spawn_shape == ESandboxNiagaraSpawnShape::Disc) {
        return 1.0f;
    }

    auto const radius_ratio{configuration.spawn_inner_radius / configuration.spawn_radius};
    return 1.0f - radius_ratio * radius_ratio;
}

auto apply_spawn_shape(UNiagaraSystem& system,
                       FName const emitter_name,
                       FSandboxNiagaraExperimentConfiguration const& configuration,
                       FSandboxNiagaraGenerationResult& result) -> bool {
    bool success{set_enum_input(system,
                                emitter_name,
                                TEXT("ParticleSpawnScript"),
                                TEXT("ShapeLocation"),
                                TEXT("Shape Primitive"),
                                spawn_primitive_display_name(configuration.spawn_shape),
                                result)};

    switch (configuration.spawn_shape) {
        case ESandboxNiagaraSpawnShape::Sphere:
            success &= set_float_input(system,
                                       emitter_name,
                                       TEXT("ParticleSpawnScript"),
                                       TEXT("ShapeLocation"),
                                       TEXT("Sphere Radius"),
                                       configuration.spawn_radius,
                                       result);
            break;
        case ESandboxNiagaraSpawnShape::Disc:
        case ESandboxNiagaraSpawnShape::Ring:
            success &= set_float_input(system,
                                       emitter_name,
                                       TEXT("ParticleSpawnScript"),
                                       TEXT("ShapeLocation"),
                                       TEXT("Ring Radius"),
                                       configuration.spawn_radius,
                                       result);
            success &= set_float_input(system,
                                       emitter_name,
                                       TEXT("ParticleSpawnScript"),
                                       TEXT("ShapeLocation"),
                                       TEXT("Disc Coverage"),
                                       ring_disc_coverage(configuration),
                                       result);
            break;
        case ESandboxNiagaraSpawnShape::Cylinder:
            success &= set_float_input(system,
                                       emitter_name,
                                       TEXT("ParticleSpawnScript"),
                                       TEXT("ShapeLocation"),
                                       TEXT("Cylinder Radius"),
                                       configuration.spawn_radius,
                                       result);
            success &= set_float_input(system,
                                       emitter_name,
                                       TEXT("ParticleSpawnScript"),
                                       TEXT("ShapeLocation"),
                                       TEXT("Cylinder Height"),
                                       configuration.spawn_height,
                                       result);
            break;
    }
    return success;
}

auto verify_spawn_shape(UNiagaraSystem& system,
                        FName const emitter_name,
                        FSandboxNiagaraExperimentConfiguration const& configuration,
                        FSandboxNiagaraGenerationResult& result) -> bool {
    bool success{verify_enum_input(system,
                                   emitter_name,
                                   TEXT("ParticleSpawnScript"),
                                   TEXT("ShapeLocation"),
                                   TEXT("Shape Primitive"),
                                   spawn_primitive_display_name(configuration.spawn_shape),
                                   result)};

    switch (configuration.spawn_shape) {
        case ESandboxNiagaraSpawnShape::Sphere:
            success &= verify_float_input(system,
                                          emitter_name,
                                          TEXT("ParticleSpawnScript"),
                                          TEXT("ShapeLocation"),
                                          TEXT("Sphere Radius"),
                                          configuration.spawn_radius,
                                          result);
            break;
        case ESandboxNiagaraSpawnShape::Disc:
        case ESandboxNiagaraSpawnShape::Ring:
            success &= verify_float_input(system,
                                          emitter_name,
                                          TEXT("ParticleSpawnScript"),
                                          TEXT("ShapeLocation"),
                                          TEXT("Ring Radius"),
                                          configuration.spawn_radius,
                                          result);
            success &= verify_float_input(system,
                                          emitter_name,
                                          TEXT("ParticleSpawnScript"),
                                          TEXT("ShapeLocation"),
                                          TEXT("Disc Coverage"),
                                          ring_disc_coverage(configuration),
                                          result);
            break;
        case ESandboxNiagaraSpawnShape::Cylinder:
            success &= verify_float_input(system,
                                          emitter_name,
                                          TEXT("ParticleSpawnScript"),
                                          TEXT("ShapeLocation"),
                                          TEXT("Cylinder Radius"),
                                          configuration.spawn_radius,
                                          result);
            success &= verify_float_input(system,
                                          emitter_name,
                                          TEXT("ParticleSpawnScript"),
                                          TEXT("ShapeLocation"),
                                          TEXT("Cylinder Height"),
                                          configuration.spawn_height,
                                          result);
            break;
    }
    return success;
}

auto apply_emission(UNiagaraSystem& system,
                    FName const emitter_name,
                    FSandboxNiagaraExperimentConfiguration const& configuration,
                    FSandboxNiagaraGenerationResult& result) -> bool {
    if (configuration.emission_mode == ESandboxNiagaraEmissionMode::Continuous) {
        return set_float_input(system,
                               emitter_name,
                               TEXT("EmitterUpdateScript"),
                               TEXT("SpawnRate"),
                               TEXT("SpawnRate"),
                               configuration.spawn_rate,
                               result);
    }

    bool success{set_float_input(system,
                                 emitter_name,
                                 TEXT("EmitterUpdateScript"),
                                 TEXT("SpawnRate"),
                                 TEXT("SpawnRate"),
                                 0.0f,
                                 result)};
    success &= set_enum_input(system,
                              emitter_name,
                              TEXT("EmitterUpdateScript"),
                              TEXT("EmitterState"),
                              TEXT("Life Cycle Mode"),
                              TEXT("Self"),
                              result);
    success &= set_enum_input(system,
                              emitter_name,
                              TEXT("EmitterUpdateScript"),
                              TEXT("EmitterState"),
                              TEXT("Inactive Response"),
                              TEXT("Complete"),
                              result);
    success &= set_enum_input(system,
                              emitter_name,
                              TEXT("EmitterUpdateScript"),
                              TEXT("EmitterState"),
                              TEXT("Loop Behavior"),
                              TEXT("Once"),
                              result);
    success &= set_enum_input(system,
                              emitter_name,
                              TEXT("EmitterUpdateScript"),
                              TEXT("EmitterState"),
                              TEXT("Loop Duration Mode"),
                              TEXT("Fixed"),
                              result);
    success &= set_float_input(system,
                               emitter_name,
                               TEXT("EmitterUpdateScript"),
                               TEXT("EmitterState"),
                               TEXT("Loop Duration"),
                               configuration.particle_lifetime,
                               result);
    if (!success) {
        return false;
    }

    auto* const burst_module{LoadObject<UNiagaraScript>(
        nullptr,
        TEXT("/Niagara/Modules/Emitter/SpawnBurstInstantaneous."
             "SpawnBurstInstantaneous"))};
    if (!IsValid(burst_module)) {
        add_error(result, TEXT("Failed to load Niagara's Spawn Burst Instantaneous module."));
        return false;
    }

    FNiagaraExt_StackItemReference burst_location{
        &system, emitter_name, TEXT("EmitterUpdateScript"), TEXT("SpawnRate")};
    FNiagaraExt_ModuleTopology burst_topology{};
    FNiagaraExternalEditContext burst_context{&system};
    UNiagaraExternalEditUtilities::AddModule(
        burst_location, burst_module, burst_topology, burst_context);
    append_context_errors(burst_context, result);
    if (burst_context.HasErrors() || burst_topology.ModuleName.IsNone()) {
        add_error(result, TEXT("Failed to add Niagara's Spawn Burst Instantaneous module."));
        return false;
    }

    success &= set_int_input(system,
                             emitter_name,
                             TEXT("EmitterUpdateScript"),
                             burst_topology.ModuleName,
                             TEXT("Spawn Count"),
                             configuration.burst_count,
                             result);
    success &= set_float_input(system,
                               emitter_name,
                               TEXT("EmitterUpdateScript"),
                               burst_topology.ModuleName,
                               TEXT("Spawn Time"),
                               0.0f,
                               result);
    return success;
}

auto verify_emission(UNiagaraSystem& system,
                     FName const emitter_name,
                     FSandboxNiagaraExperimentConfiguration const& configuration,
                     FSandboxNiagaraGenerationResult& result) -> bool {
    if (configuration.emission_mode == ESandboxNiagaraEmissionMode::Continuous) {
        return verify_float_input(system,
                                  emitter_name,
                                  TEXT("EmitterUpdateScript"),
                                  TEXT("SpawnRate"),
                                  TEXT("SpawnRate"),
                                  configuration.spawn_rate,
                                  result);
    }

    bool success{verify_float_input(system,
                                    emitter_name,
                                    TEXT("EmitterUpdateScript"),
                                    TEXT("SpawnRate"),
                                    TEXT("SpawnRate"),
                                    0.0f,
                                    result)};
    success &= verify_enum_input(system,
                                 emitter_name,
                                 TEXT("EmitterUpdateScript"),
                                 TEXT("EmitterState"),
                                 TEXT("Life Cycle Mode"),
                                 TEXT("Self"),
                                 result);
    success &= verify_enum_input(system,
                                 emitter_name,
                                 TEXT("EmitterUpdateScript"),
                                 TEXT("EmitterState"),
                                 TEXT("Inactive Response"),
                                 TEXT("Complete"),
                                 result);
    success &= verify_enum_input(system,
                                 emitter_name,
                                 TEXT("EmitterUpdateScript"),
                                 TEXT("EmitterState"),
                                 TEXT("Loop Behavior"),
                                 TEXT("Once"),
                                 result);
    success &= verify_enum_input(system,
                                 emitter_name,
                                 TEXT("EmitterUpdateScript"),
                                 TEXT("EmitterState"),
                                 TEXT("Loop Duration Mode"),
                                 TEXT("Fixed"),
                                 result);
    success &= verify_float_input(system,
                                  emitter_name,
                                  TEXT("EmitterUpdateScript"),
                                  TEXT("EmitterState"),
                                  TEXT("Loop Duration"),
                                  configuration.particle_lifetime,
                                  result);

    FNiagaraExt_StackItemReference const emitter_ref{&system, emitter_name};
    FNiagaraExt_EmitterTopology topology{};
    FNiagaraExternalEditContext topology_context{&system};
    UNiagaraExternalEditUtilities::GetEmitterTopology(
        emitter_ref, topology, topology_context);
    append_context_errors(topology_context, result);
    auto const* burst_module{topology.EmitterUpdateScript.Modules.FindByPredicate(
        [](FNiagaraExt_ModuleTopology const& module) {
            return module.ModuleScript != nullptr &&
                   module.ModuleScript->GetPathName() ==
                       TEXT("/Niagara/Modules/Emitter/SpawnBurstInstantaneous."
                            "SpawnBurstInstantaneous");
        })};
    if (topology_context.HasErrors() || burst_module == nullptr) {
        add_error(result, TEXT("Generated system does not contain Spawn Burst Instantaneous."));
        return false;
    }

    success &= verify_int_input(system,
                                emitter_name,
                                TEXT("EmitterUpdateScript"),
                                burst_module->ModuleName,
                                TEXT("Spawn Count"),
                                configuration.burst_count,
                                result);
    success &= verify_float_input(system,
                                  emitter_name,
                                  TEXT("EmitterUpdateScript"),
                                  burst_module->ModuleName,
                                  TEXT("Spawn Time"),
                                  0.0f,
                                  result);
    return success;
}

auto apply_baseline_configuration(UNiagaraSystem& system,
                                  FSandboxNiagaraExperimentConfiguration const& configuration,
                                  FSandboxNiagaraGenerationResult& result) -> bool {
    if (!FMath::IsFinite(configuration.spawn_rate) ||
        !FMath::IsFinite(configuration.particle_lifetime) ||
        !FMath::IsFinite(configuration.sprite_size) ||
        !FMath::IsFinite(configuration.spawn_radius) ||
        !FMath::IsFinite(configuration.spawn_height) ||
        !FMath::IsFinite(configuration.spawn_inner_radius) ||
        !FMath::IsFinite(configuration.fixed_bounds_extent) ||
        configuration.spawn_rate < 0.0f || configuration.particle_lifetime <= 0.0f ||
        configuration.sprite_size <= 0.0f || configuration.spawn_radius < 0.0f ||
        configuration.spawn_height <= 0.0f || configuration.spawn_inner_radius < 0.0f ||
        configuration.fixed_bounds_extent <= 0.0f) {
        add_error(result,
                  TEXT("Spawn rate, lifetime, sprite size, shape dimensions, and fixed-bounds "
                       "extent must be finite. Spawn rate, radius, and inner radius must be "
                       "non-negative; lifetime, sprite size, height, and fixed-bounds extent "
                       "must be positive."));
        return false;
    }
    if (configuration.emission_mode == ESandboxNiagaraEmissionMode::Burst &&
        configuration.burst_count <= 0) {
        add_error(result, TEXT("A burst emission requires a positive particle count."));
        return false;
    }
    if (configuration.spawn_shape == ESandboxNiagaraSpawnShape::Ring &&
        (configuration.spawn_radius <= 0.0f ||
         configuration.spawn_inner_radius >= configuration.spawn_radius)) {
        add_error(result,
                  TEXT("A ring spawn shape requires a positive radius and an inner radius smaller "
                       "than its outer radius."));
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
    emitter_data->bLocalSpace = configuration.local_space;
    emitter_data->CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Fixed;
    auto const bounds_extent{FVector{configuration.fixed_bounds_extent}};
    emitter_data->FixedBounds = FBox{-bounds_extent, bounds_extent};
    emitter->PostEditChange();
    system.PostEditChange();

    auto const emitter_name{emitter_handle.GetName()};
    bool success{true};
    success &= apply_emission(system, emitter_name, configuration, result);
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
                               configuration.particle_color,
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
    success &= apply_spawn_shape(system, emitter_name, configuration, result);
    success &= set_bool_input(system,
                              emitter_name,
                              TEXT("ParticleUpdateScript"),
                              TEXT("ParticleState"),
                              TEXT("Kill Particles When Lifetime Has Elapsed"),
                              true,
                              result);
    if (configuration.emission_mode == ESandboxNiagaraEmissionMode::Burst) {
        auto* const editor_data{Cast<UNiagaraSystemEditorData>(system.GetEditorData())};
        if (editor_data == nullptr) {
            add_error(result, TEXT("Generated burst system does not have Niagara editor data."));
            success = false;
        } else {
            editor_data->Modify();
            editor_data->SetPlaybackRange(
                TRange<float>{0.0f, configuration.particle_lifetime + burst_preview_tail});
        }
    }

    return success;
}

auto add_float_user_parameters(
    UNiagaraSystem& system,
    TConstArrayView<FSandboxNiagaraFloatParameter> const parameters,
    FSandboxNiagaraGenerationResult& result) -> bool {
    TSet<FName> parameter_names{};
    bool success{true};
    for (FSandboxNiagaraFloatParameter const& parameter : parameters) {
        if (!FMath::IsFinite(parameter.value) || !FMath::IsFinite(parameter.minimum) ||
            !FMath::IsFinite(parameter.maximum) || parameter.minimum > parameter.maximum ||
            parameter.value < parameter.minimum || parameter.value > parameter.maximum) {
            add_error(result,
                      FString::Printf(TEXT("Float parameter '%s' must have finite, ordered bounds "
                                           "and a value inside those bounds."),
                                      *parameter.name.ToString()));
            success = false;
            continue;
        }
        if (parameter.name.IsNone() ||
            !parameter.name.ToString().StartsWith(TEXT("User."))) {
            add_error(result,
                      FString::Printf(TEXT("Float parameter '%s' must use the User namespace."),
                                      *parameter.name.ToString()));
            success = false;
            continue;
        }
        if (parameter_names.Contains(parameter.name)) {
            add_error(result,
                      FString::Printf(TEXT("Float parameter '%s' is defined more than once."),
                                      *parameter.name.ToString()));
            success = false;
            continue;
        }
        parameter_names.Add(parameter.name);

        FNiagaraExt_UserVariable variable{};
        variable.Name = parameter.name;
        variable.Type = FNiagaraTypeDefinition::GetFloatDef();
        variable.DefaultValue.InitializeAs<FNiagaraFloat>().Value = parameter.value;
        variable.Description = FText::FromString(TEXT("Generated by SandboxNiagara."));

        FNiagaraExternalEditContext context{&system};
        UNiagaraExternalEditUtilities::AddUserVariable(&system, variable, context);
        append_context_errors(context, result);
        success &= !context.HasErrors();
    }
    return success;
}

auto add_particle_velocity_update(UNiagaraSystem& system,
                                  FString const& velocity_expression,
                                  FSandboxNiagaraGenerationResult& result) -> bool {
    if (velocity_expression.IsEmpty()) {
        return true;
    }

    auto const& emitter_handles{system.GetEmitterHandles()};
    if (emitter_handles.Num() != 1) {
        add_error(result, TEXT("Cannot add particle update logic without exactly one emitter."));
        return false;
    }

    auto const emitter_name{emitter_handles[0].GetName()};
    FNiagaraExt_SetParameterEntry velocity_entry{};
    velocity_entry.Variable.Name = TEXT("Particles.Velocity");
    velocity_entry.Variable.Type = FNiagaraTypeDefinition::GetVec3Def();
    auto const default_velocity{FVector3f::ZeroVector};
    velocity_entry.DefaultValue.InitializeAs(
        velocity_entry.Variable.Type.GetScriptStruct(),
        reinterpret_cast<uint8 const*>(&default_velocity));

    FNiagaraExt_StackItemReference assignment_location{
        &system, emitter_name, TEXT("ParticleUpdateScript"), TEXT("ParticleState")};
    FNiagaraExt_ModuleTopology assignment_topology{};
    FNiagaraExternalEditContext assignment_context{&system};
    UNiagaraExternalEditUtilities::AddSetParametersModule(
        assignment_location, {velocity_entry}, assignment_topology, assignment_context);
    append_context_errors(assignment_context, result);
    if (assignment_context.HasErrors() || assignment_topology.ModuleName.IsNone()) {
        add_error(result, TEXT("Failed to add the particle velocity Set Parameters module."));
        return false;
    }

    if (!set_hlsl_input(system,
                        emitter_name,
                        TEXT("ParticleUpdateScript"),
                        assignment_topology.ModuleName,
                        velocity_entry.Variable.Name,
                        velocity_expression,
                        result)) {
        return false;
    }

    auto* const solver_module{LoadObject<UNiagaraScript>(
        nullptr,
        TEXT("/Niagara/Modules/Solvers/SolveForcesAndVelocity.SolveForcesAndVelocity"))};
    if (!IsValid(solver_module)) {
        add_error(result, TEXT("Failed to load Niagara's Solve Forces and Velocity module."));
        return false;
    }

    FNiagaraExt_StackItemReference solver_location{&system,
                                                   emitter_name,
                                                   TEXT("ParticleUpdateScript"),
                                                   assignment_topology.ModuleName};
    FNiagaraExt_ModuleTopology solver_topology{};
    FNiagaraExternalEditContext solver_context{&system};
    UNiagaraExternalEditUtilities::AddModule(
        solver_location, solver_module, solver_topology, solver_context);
    append_context_errors(solver_context, result);
    if (solver_context.HasErrors() || solver_topology.ModuleName.IsNone()) {
        add_error(result, TEXT("Failed to add Niagara's Solve Forces and Velocity module."));
        return false;
    }

    UE_LOG(LogSandboxNiagara,
           Display,
           TEXT("Added particle update modules %s and %s."),
           *assignment_topology.ModuleName.ToString(),
           *solver_topology.ModuleName.ToString());
    return true;
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
    if (emitter_data->bLocalSpace != configuration.local_space) {
        add_error(result, TEXT("Generated SandboxEmitter uses the wrong simulation space."));
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
    success &= verify_emission(system, emitter_name, configuration, result);
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
                                  configuration.particle_color,
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
    success &= verify_spawn_shape(system, emitter_name, configuration, result);
    success &= verify_bool_input(system,
                                 emitter_name,
                                 TEXT("ParticleUpdateScript"),
                                 TEXT("ParticleState"),
                                 TEXT("Kill Particles When Lifetime Has Elapsed"),
                                 true,
                                 result);
    if (configuration.emission_mode == ESandboxNiagaraEmissionMode::Burst) {
        auto const* const editor_data{
            Cast<UNiagaraSystemEditorData>(system.GetEditorData())};
        auto const expected_playback_end{configuration.particle_lifetime + burst_preview_tail};
        if (editor_data == nullptr || !editor_data->GetPlaybackRange().HasLowerBound() ||
            !editor_data->GetPlaybackRange().HasUpperBound() ||
            !FMath::IsNearlyZero(editor_data->GetPlaybackRange().GetLowerBoundValue()) ||
            !FMath::IsNearlyEqual(editor_data->GetPlaybackRange().GetUpperBoundValue(),
                                  expected_playback_end)) {
            add_error(result,
                      TEXT("Generated burst system does not have the configured editor playback range."));
            success = false;
        }
    }

    return success;
}

auto verify_float_user_parameters(
    UNiagaraSystem& system,
    TConstArrayView<FSandboxNiagaraFloatParameter> const expected_parameters,
    FSandboxNiagaraGenerationResult& result) -> bool {
    if (expected_parameters.IsEmpty()) {
        return true;
    }

    FNiagaraExt_UserVariables variables{};
    FNiagaraExternalEditContext context{&system};
    UNiagaraExternalEditUtilities::GetUserVariables(&system, variables, context);
    append_context_errors(context, result);
    if (context.HasErrors()) {
        return false;
    }

    bool success{true};
    for (FSandboxNiagaraFloatParameter const& expected : expected_parameters) {
        FNiagaraExt_UserVariable const* actual{variables.UserVariables.FindByPredicate(
            [&expected](FNiagaraExt_UserVariable const& candidate) {
                return candidate.Name == expected.name;
            })};
        auto const* actual_value{
            actual != nullptr ? actual->DefaultValue.GetPtr<FNiagaraFloat>() : nullptr};
        if (actual == nullptr || actual->Type != FNiagaraTypeDefinition::GetFloatDef() ||
            actual_value == nullptr ||
            !FMath::IsNearlyEqual(actual_value->Value, expected.value)) {
            add_error(result,
                      FString::Printf(TEXT("Generated user parameter %s did not retain value %g."),
                                      *expected.name.ToString(),
                                      expected.value));
            success = false;
        }
    }
    return success;
}

auto verify_particle_velocity_update(UNiagaraSystem& system,
                                     FString const& expected_expression,
                                     FSandboxNiagaraGenerationResult& result) -> bool {
    if (expected_expression.IsEmpty()) {
        return true;
    }

    auto const& emitter_handles{system.GetEmitterHandles()};
    if (emitter_handles.Num() != 1) {
        return false;
    }

    auto const emitter_name{emitter_handles[0].GetName()};
    FNiagaraExt_StackItemReference const emitter_ref{&system, emitter_name};
    FNiagaraExt_EmitterTopology topology{};
    FNiagaraExternalEditContext topology_context{&system};
    UNiagaraExternalEditUtilities::GetEmitterTopology(
        emitter_ref, topology, topology_context);
    append_context_errors(topology_context, result);
    if (topology_context.HasErrors()) {
        return false;
    }

    auto const& update_modules{topology.ParticleUpdateScript.Modules};
    if (update_modules.Num() != 3 || update_modules[0].ModuleName != TEXT("ParticleState") ||
        !update_modules[1].bIsSetParametersModule ||
        update_modules[2].ModuleScript == nullptr ||
        update_modules[2].ModuleScript->GetPathName() !=
            TEXT("/Niagara/Modules/Solvers/SolveForcesAndVelocity.SolveForcesAndVelocity")) {
        add_error(result,
                  TEXT("Generated Particle Update stack must contain Particle State, the velocity "
                       "Set Parameters module, and Solve Forces and Velocity in that order."));
        return false;
    }

    FNiagaraExt_StackInputValue value{};
    if (!get_stack_input(system,
                         emitter_name,
                         TEXT("ParticleUpdateScript"),
                         update_modules[1].ModuleName,
                         TEXT("Particles.Velocity"),
                         value,
                         result)) {
        return false;
    }

    auto const* expression{value.GetPtr<FNiagaraExt_StackInputData_HlslExpression>()};
    if (expression == nullptr || expression->HlslExpression != expected_expression) {
        add_error(result,
                  TEXT("Generated Particles.Velocity did not retain its custom HLSL expression."));
        return false;
    }
    return true;
}
}

FSandboxNiagaraExperimentCatalogResult
USandboxNiagaraSubsystem::load_experiment_catalog() const {
    FSandboxNiagaraExperimentCatalogResult result{};
    auto const plugin{IPluginManager::Get().FindPlugin(TEXT("SandboxNiagara"))};
    if (!plugin.IsValid()) {
        add_catalog_error(result,
                          TEXT("SandboxNiagara"),
                          TEXT("Plugin descriptor could not be found."));
        return result;
    }

    auto const experiments_directory{FPaths::ConvertRelativePathToFull(
        FPaths::Combine(plugin->GetBaseDir(), TEXT("Experiments")))};
    TArray<FString> definition_names{};
    IFileManager::Get().FindFiles(
        definition_names, *(experiments_directory / TEXT("*.json")), true, false);
    definition_names.Sort();
    if (definition_names.IsEmpty()) {
        add_catalog_error(result,
                          experiments_directory,
                          TEXT("No experiment JSON files were found."));
        return result;
    }

    TSet<FString> ids{};
    TSet<FString> asset_names{};
    for (FString const& definition_name : definition_names) {
        FSandboxNiagaraExperimentDefinition definition{};
        auto const definition_path{experiments_directory / definition_name};
        if (!load_experiment_definition(
                definition_path, experiments_directory, definition, result)) {
            continue;
        }

        auto const normalized_id{definition.id.ToLower()};
        auto const normalized_asset_name{definition.default_asset_name.ToLower()};
        if (ids.Contains(normalized_id)) {
            add_catalog_error(result,
                              definition_path,
                              FString::Printf(TEXT("Experiment id '%s' is duplicated."),
                                              *definition.id));
            continue;
        }
        if (asset_names.Contains(normalized_asset_name)) {
            add_catalog_error(result,
                              definition_path,
                              FString::Printf(TEXT("Default asset name '%s' is duplicated."),
                                              *definition.default_asset_name));
            continue;
        }
        ids.Add(normalized_id);
        asset_names.Add(normalized_asset_name);
        result.experiments.Add(MoveTemp(definition));
    }

    result.success = result.errors.IsEmpty() && !result.experiments.IsEmpty();
    if (result.success) {
        UE_LOG(LogSandboxNiagara,
               Display,
               TEXT("Loaded %d text-defined Niagara experiment(s) from %s."),
               result.experiments.Num(),
               *experiments_directory);
    }
    return result;
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

FSandboxNiagaraGenerationResult USandboxNiagaraSubsystem::generate_experiment_internal(
    UNiagaraSystem* template_system,
    FString const& experiment_name,
    FSandboxNiagaraExperimentConfiguration const& configuration,
    bool const replace_existing) {
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

    FSandboxNiagaraGenerationResult template_diagnostics{};
    if (!validate_template_structure(*template_system, template_diagnostics)) {
        add_error(result,
                  FString::Printf(TEXT("Template Niagara System is invalid: %s"),
                                  *template_system->GetPathName()));
        result.errors.Append(template_diagnostics.errors);
        result.warnings.Append(template_diagnostics.warnings);
        return result;
    }

    auto const registered_asset_exists{
        asset_subsystem->DoesAssetExist(destination_object_path)};
    auto const package_file_exists{
        FPackageName::DoesPackageExist(destination_package_path)};
    auto* loaded_destination{FindObject<UObject>(nullptr, *destination_object_path)};
    if (loaded_destination == nullptr && package_file_exists && !registered_asset_exists) {
        loaded_destination =
            LoadObject<UObject>(nullptr, *destination_object_path, nullptr, LOAD_NoWarn);
    }
    auto const asset_exists{loaded_destination != nullptr || registered_asset_exists ||
                            package_file_exists};
    if (!replace_existing &&
        (asset_exists || FindPackage(nullptr, *destination_package_path) != nullptr)) {
        add_error(result,
                  FString::Printf(TEXT("Generated asset already exists: %s"),
                                  *destination_object_path));
        return result;
    }

    if (replace_existing && !asset_exists &&
        FindPackage(nullptr, *destination_package_path) != nullptr) {
        add_error(result,
                  FString::Printf(TEXT("Cannot regenerate %s because an unsaved package with that "
                                       "name is already loaded."),
                                  *destination_object_path));
        return result;
    }

    auto const use_staging_asset{replace_existing && asset_exists};
    auto const creation_name{
        use_staging_asset
            ? FString::Printf(TEXT("%s__Regenerating_%s"),
                              *experiment_name,
                              *FGuid::NewGuid().ToString(EGuidFormats::Digits))
            : experiment_name};
    auto const creation_package_path{output_path / creation_name};
    auto const creation_object_path{
        FString::Printf(TEXT("%s.%s"), *creation_package_path, *creation_name)};

    UE_LOG(LogSandboxNiagara,
           Display,
           TEXT("Generating Niagara experiment %s from %s."),
           *creation_object_path,
           *template_system->GetPathName());

    FNiagaraExternalEditContext creation_context{};
    auto* const generated_system{UNiagaraExternalEditUtilities::CreateNiagaraSystem(
        creation_name, output_path, template_system, creation_context)};
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
    if (!add_float_user_parameters(
            *generated_system, configuration.float_parameters, result)) {
        add_error(result,
                  FString::Printf(TEXT("Failed to add user parameters to generated Niagara "
                                       "System: %s"),
                                  *generated_system->GetPathName()));
    }
    if (!add_particle_velocity_update(
            *generated_system, configuration.particle_velocity_expression, result)) {
        add_error(result,
                  FString::Printf(TEXT("Failed to add particle update logic to generated Niagara "
                                       "System: %s"),
                                  *generated_system->GetPathName()));
    }

    generated_system->RequestCompile(true);
    generated_system->WaitForCompilationComplete(true, false);
    auto const compile_success{collect_compile_diagnostics(*generated_system, result)};
    auto const configuration_verified{
        verify_baseline_configuration(*generated_system, configuration, result)};
    auto const parameters_verified{verify_float_user_parameters(
        *generated_system, configuration.float_parameters, result)};
    auto const particle_update_verified{verify_particle_velocity_update(
        *generated_system, configuration.particle_velocity_expression, result)};

    auto save_success{false};
    auto const generation_verified{compile_success && configuration_verified &&
                                   parameters_verified && particle_update_verified &&
                                   result.errors.IsEmpty()};
    if (generation_verified) {
        save_success = asset_subsystem->SaveLoadedAsset(generated_system, false);
    }
    if (!save_success) {
        add_error(result,
                  FString::Printf(TEXT("Failed to save generated Niagara System: %s"),
                                  *generated_system->GetPathName()));
    }

    if (save_success && use_staging_asset) {
        UE_LOG(LogSandboxNiagara,
               Display,
               TEXT("Replacing generated Niagara experiment after successful validation: %s"),
               *destination_object_path);
        if (!asset_subsystem->DeleteAsset(destination_object_path)) {
            add_error(result,
                      FString::Printf(TEXT("The regenerated asset is valid at %s, but the previous "
                                           "asset could not be deleted: %s"),
                                      *creation_object_path,
                                      *destination_object_path));
        } else {
            generated_system->AddToRoot();
            CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

            FString destination_filename{};
            auto const has_destination_filename{
                FPackageName::TryConvertLongPackageNameToFilename(
                    destination_package_path,
                    destination_filename,
                    FPackageName::GetAssetPackageExtension())};
            if (has_destination_filename && IFileManager::Get().FileExists(*destination_filename) &&
                !IFileManager::Get().Delete(*destination_filename, false, true)) {
                generated_system->RemoveFromRoot();
                add_error(result,
                          FString::Printf(TEXT("The regenerated asset is valid at %s, but the old "
                                               "generated package file could not be removed: %s"),
                                          *creation_object_path,
                                          *destination_filename));
                return result;
            }

            auto const rename_success{asset_subsystem->RenameAsset(
                creation_object_path, destination_object_path)};
            generated_system->RemoveFromRoot();

            if (!rename_success) {
            add_error(result,
                      FString::Printf(TEXT("The regenerated asset is valid at %s, but it could not "
                                           "be promoted to %s."),
                                      *creation_object_path,
                                      *destination_object_path));
            } else {
                result.generated_asset_path = generated_system->GetPathName();
            }
        }
    }

    result.success = save_success && result.errors.IsEmpty() &&
                     (!use_staging_asset ||
                      result.generated_asset_path == destination_object_path);
    if (result.success) {
        UE_LOG(LogSandboxNiagara,
               Display,
               TEXT("Generated Niagara experiment %s with compile status %s."),
               *generated_system->GetPathName(),
               *result.compile_status);
    }

    return result;
}

FSandboxNiagaraGenerationResult USandboxNiagaraSubsystem::generate_experiment(
    UNiagaraSystem* template_system,
    FString const& experiment_name,
    FSandboxNiagaraExperimentConfiguration const& configuration) {
    return generate_experiment_internal(
        template_system, experiment_name, configuration, false);
}

FSandboxNiagaraGenerationResult USandboxNiagaraSubsystem::regenerate_experiment(
    UNiagaraSystem* template_system,
    FString const& experiment_name,
    FSandboxNiagaraExperimentConfiguration const& configuration) {
    return generate_experiment_internal(
        template_system, experiment_name, configuration, true);
}

FSandboxNiagaraValidationResult
USandboxNiagaraSubsystem::delete_generated_asset(FString const& asset_path) {
    FSandboxNiagaraValidationResult result{};
    auto package_path{FPackageName::ObjectPathToPackageName(asset_path)};
    if (package_path.IsEmpty() && FPackageName::IsValidLongPackageName(asset_path)) {
        package_path = asset_path;
    }

    auto const output_path{FPackageName::GetLongPackagePath(package_path)};
    auto const asset_name{FPackageName::GetLongPackageAssetName(package_path)};
    if (package_path.IsEmpty() || asset_name.IsEmpty() || !is_generated_output_path(output_path)) {
        result.errors.Add(FString::Printf(
            TEXT("Only assets inside %s may be deleted. Received '%s'."),
            *generated_output_root,
            *asset_path));
        return result;
    }
    if (GEditor == nullptr) {
        result.errors.Add(TEXT("The Unreal Editor is unavailable."));
        return result;
    }

    auto* const asset_subsystem{GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()};
    if (asset_subsystem == nullptr) {
        result.errors.Add(TEXT("The Editor Asset Subsystem is unavailable."));
        return result;
    }

    auto const object_path{FString::Printf(TEXT("%s.%s"), *package_path, *asset_name)};
    if (!asset_subsystem->DoesAssetExist(object_path)) {
        result.errors.Add(FString::Printf(TEXT("Generated asset does not exist: %s"),
                                         *object_path));
        return result;
    }
    if (!asset_subsystem->DeleteAsset(object_path)) {
        result.errors.Add(FString::Printf(TEXT("Failed to delete generated asset: %s"),
                                         *object_path));
        return result;
    }

    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    FString package_filename{};
    if (FPackageName::TryConvertLongPackageNameToFilename(
            package_path,
            package_filename,
            FPackageName::GetAssetPackageExtension()) &&
        IFileManager::Get().FileExists(*package_filename) &&
        !IFileManager::Get().Delete(*package_filename, false, true)) {
        result.errors.Add(FString::Printf(TEXT("Failed to remove generated package file: %s"),
                                         *package_filename));
        return result;
    }

    UE_LOG(LogSandboxNiagara, Display, TEXT("Deleted generated asset %s."), *object_path);
    result.success = true;
    return result;
}

FSandboxNiagaraValidationResult
USandboxNiagaraSubsystem::regenerate_all_experiments(UNiagaraSystem* template_system) {
    FSandboxNiagaraValidationResult result{};
    auto const catalog{load_experiment_catalog()};
    result.warnings.Append(catalog.warnings);
    result.errors.Append(catalog.errors);
    if (!catalog.success) {
        return result;
    }

    result.success = true;
    for (FSandboxNiagaraExperimentDefinition const& experiment : catalog.experiments) {
        auto const generation_result{generate_experiment_internal(template_system,
                                                                  experiment.default_asset_name,
                                                                  experiment.configuration,
                                                                  true)};
        result.success &= generation_result.success;
        result.warnings.Append(generation_result.warnings);
        result.errors.Append(generation_result.errors);
    }
    return result;
}

FSandboxNiagaraValidationResult USandboxNiagaraSubsystem::create_or_refresh_showcase() {
    FSandboxNiagaraValidationResult result{};
    auto const catalog{load_experiment_catalog()};
    result.warnings.Append(catalog.warnings);
    result.errors.Append(catalog.errors);
    if (!catalog.success) {
        return result;
    }
    if (GEditor == nullptr) {
        add_validation_error(result, TEXT("The Unreal Editor is unavailable."));
        return result;
    }
    TArray<FSandboxNiagaraExperimentDefinition> available_experiments{};
    TArray<UNiagaraSystem*> available_systems{};
    available_experiments.Reserve(catalog.experiments.Num());
    available_systems.Reserve(catalog.experiments.Num());
    for (FSandboxNiagaraExperimentDefinition const& experiment : catalog.experiments) {
        auto const output_path{normalize_output_path(experiment.configuration.output_path)};
        auto const system_path{FString::Printf(TEXT("%s/%s.%s"),
                                               *output_path,
                                               *experiment.default_asset_name,
                                               *experiment.default_asset_name)};
        auto* const system{
            LoadObject<UNiagaraSystem>(nullptr, *system_path, nullptr, LOAD_NoWarn)};
        if (!IsValid(system)) {
            auto const warning{
                FString::Printf(TEXT("Skipping missing showcase system: %s"), *system_path)};
            UE_LOG(LogSandboxNiagara, Warning, TEXT("%s"), *warning);
            result.warnings.Add(warning);
            continue;
        }

        available_experiments.Add(experiment);
        available_systems.Add(system);
    }
    if (available_experiments.IsEmpty()) {
        add_validation_error(result, TEXT("No generated Niagara systems are available to showcase."));
        return result;
    }

    auto const layout{build_showcase_layout(available_experiments)};
    TArray<FSandboxNiagaraShowcaseEntry> entries{};
    entries.Reserve(available_experiments.Num());
    auto const available_count{available_experiments.Num()};
    for (int32 experiment_index{0}; experiment_index < available_count; ++experiment_index) {
        auto const& experiment{available_experiments[experiment_index]};
        auto const effect_location{layout.effect_locations[experiment_index]};
        auto const effect_scale{
            experiment.configuration.emission_mode == ESandboxNiagaraEmissionMode::Burst
                ? showcase_burst_scale
                : 1.0f};
        auto const label_location{
            effect_location +
            FVector{0.0f,
                    0.0f,
                    experiment.configuration.fixed_bounds_extent * effect_scale +
                        showcase_label_padding}};
        auto const label_rotation{(layout.camera_location - label_location).Rotation()};
        FSandboxNiagaraShowcaseEntry entry{};
        entry.system = available_systems[experiment_index];
        entry.display_name = experiment.display_name;
        entry.effect_transform =
            FTransform{FQuat::Identity, effect_location, FVector{effect_scale}};
        entry.label_transform = FTransform{label_rotation, label_location};
        entry.replay_burst =
            experiment.configuration.emission_mode == ESandboxNiagaraEmissionMode::Burst;
        entries.Add(MoveTemp(entry));
    }
    if (!UEditorLoadingAndSavingUtils::SaveDirtyPackagesWithDialog(true, false)) {
        add_validation_error(
            result,
            TEXT("Showcase creation was cancelled while saving the current map."));
        return result;
    }

    auto* const level_editor{GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()};
    if (!IsValid(level_editor)) {
        add_validation_error(result, TEXT("The Level Editor subsystem is unavailable."));
        return result;
    }
    auto const map_exists{FPackageName::DoesPackageExist(showcase_package_path)};
    auto const map_loaded{map_exists ? level_editor->LoadLevel(showcase_package_path)
                                     : level_editor->NewLevel(showcase_package_path)};
    if (!map_loaded) {
        add_validation_error(
            result,
            FString::Printf(TEXT("Could not %s the showcase map: %s"),
                            map_exists ? TEXT("load") : TEXT("create"),
                            *showcase_package_path));
        return result;
    }

    auto* const world{GEditor->GetEditorWorldContext().World()};
    if (!IsValid(world)) {
        add_validation_error(result, TEXT("The showcase editor world is unavailable."));
        return result;
    }

    TArray<AActor*> actors_to_destroy{};
    for (TActorIterator<AActor> actor_iterator{world}; actor_iterator; ++actor_iterator) {
        auto* const actor{*actor_iterator};
        if (IsValid(actor) && !actor->IsA<AWorldSettings>()) {
            actors_to_destroy.Add(actor);
        }
    }
    for (auto* const actor : actors_to_destroy) {
        world->EditorDestroyActor(actor, true);
    }

    FActorSpawnParameters showcase_spawn_parameters{};
    showcase_spawn_parameters.ObjectFlags |= RF_Transactional;
    auto* const showcase_actor{world->SpawnActor<ASandboxNiagaraShowcaseActor>(
        ASandboxNiagaraShowcaseActor::StaticClass(),
        FTransform::Identity,
        showcase_spawn_parameters)};
    if (!IsValid(showcase_actor)) {
        add_validation_error(result, TEXT("Could not spawn the Niagara showcase actor."));
        return result;
    }
    showcase_actor->SetActorLabel(TEXT("Sandbox Niagara Showcase"));
    showcase_actor->configure(MoveTemp(entries), showcase_replay_interval);

    FActorSpawnParameters camera_spawn_parameters{};
    camera_spawn_parameters.ObjectFlags |= RF_Transactional;
    auto* const camera{world->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),
                                                      layout.camera_location,
                                                      layout.camera_rotation,
                                                      camera_spawn_parameters)};
    if (!IsValid(camera)) {
        add_validation_error(result, TEXT("Could not spawn the showcase overview camera."));
        return result;
    }
    camera->SetActorLabel(TEXT("Sandbox Niagara Showcase Camera"));
    camera->Tags.Add(showcase_camera_tag);

    if (!UEditorLoadingAndSavingUtils::SaveMap(world, showcase_package_path)) {
        add_validation_error(
            result,
            FString::Printf(TEXT("Could not save the showcase map: %s"),
                            *showcase_package_path));
        return result;
    }

    frame_showcase_camera(*world, result);
    result.success = result.errors.IsEmpty();
    if (result.success) {
        UE_LOG(LogSandboxNiagara,
               Display,
               TEXT("Created Niagara showcase with %d effects at %s."),
               available_count,
               *showcase_package_path);
    }
    return result;
}

FSandboxNiagaraValidationResult USandboxNiagaraSubsystem::open_showcase() {
    FSandboxNiagaraValidationResult result{};
    if (GEditor == nullptr) {
        add_validation_error(result, TEXT("The Unreal Editor is unavailable."));
        return result;
    }
    if (!FPackageName::DoesPackageExist(showcase_package_path)) {
        add_validation_error(
            result,
            FString::Printf(TEXT("The showcase has not been generated yet: %s"),
                            *showcase_object_path));
        return result;
    }
    if (!UEditorLoadingAndSavingUtils::SaveDirtyPackagesWithDialog(true, false)) {
        add_validation_error(result, TEXT("Opening the showcase was cancelled while saving the current map."));
        return result;
    }

    auto* const level_editor{GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()};
    if (!IsValid(level_editor) || !level_editor->LoadLevel(showcase_package_path)) {
        add_validation_error(
            result,
            FString::Printf(TEXT("Could not open the showcase map: %s"),
                            *showcase_package_path));
        return result;
    }

    auto* const world{GEditor->GetEditorWorldContext().World()};
    if (!IsValid(world)) {
        add_validation_error(result, TEXT("The showcase editor world is unavailable."));
        return result;
    }
    frame_showcase_camera(*world, result);
    result.success = result.errors.IsEmpty();
    return result;
}
