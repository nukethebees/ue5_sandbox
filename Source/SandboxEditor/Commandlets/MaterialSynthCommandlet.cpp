#include "SandboxEditor/Commandlets/MaterialSynthCommandlet.h"

#include "SandboxEditor/material/MaterialEmitter.h"

#include <material_gen/CompiledMaterial.h>

#include "Engine/Texture.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RHIGlobals.h"

namespace {

void print_result(bool const success,
                  bool const validate,
                  FString const& asset,
                  int32 const error_count) {
    UE_LOG(LogTemp,
           Display,
           TEXT("MATERIAL_SYNTH_RESULT\nstatus=%s\nmode=%s\nasset=%s\nerrors=%d"),
           success ? TEXT("success") : TEXT("failure"),
           validate ? TEXT("validate") : TEXT("generate"),
           *asset,
           error_count);
}

auto texture_object_path(std::string_view const source_path) -> FString {
    auto path{FString{UTF8_TO_TCHAR(source_path.data())}};
    if (!path.Contains(TEXT("."))) {
        auto leaf{path};
        int32 slash{};
        if (leaf.FindLastChar(TEXT('/'), slash)) {
            leaf.RightChopInline(slash + 1);
        }
        path += TEXT(".") + leaf;
    }
    return path;
}

}

UMaterialSynthCommandlet::UMaterialSynthCommandlet() {
    IsClient = true;
    IsEditor = true;
    LogToConsole = true;
}

int32 UMaterialSynthCommandlet::Main(FString const& parameters) {
    bool const validate_only{FParse::Param(*parameters, TEXT("Validate"))};
    if (FParse::Param(*parameters, TEXT("GenerateAll"))) {
        FString artifact_directory;
        FString artifact_list;
        if (!FParse::Value(*parameters, TEXT("ArtifactDirectory="), artifact_directory, false) ||
            !FParse::Value(*parameters, TEXT("ArtifactList="), artifact_list, false)) {
            UE_LOG(LogTemp,
                   Error,
                   TEXT("MaterialSynth -GenerateAll requires -ArtifactDirectory and "
                        "-ArtifactList."));
            print_result(false, validate_only, {}, 1);
            return 1;
        }

        TArray<FString> artifact_names;
        artifact_list.ParseIntoArray(artifact_names, TEXT(","), true);
        if (artifact_names.IsEmpty()) {
            UE_LOG(LogTemp, Error, TEXT("MaterialSynth -ArtifactList must not be empty."));
            print_result(false, validate_only, {}, 1);
            return 1;
        }

        int32 failures{};
        for (auto const& artifact_name : artifact_names) {
            auto const artifact_path{
                FPaths::Combine(artifact_directory, artifact_name + TEXT(".smat"))};
            auto const artifact_parameters{
                FString::Printf(TEXT("-Artifact=\"%s\" %s"),
                                *artifact_path,
                                validate_only ? TEXT("-Validate") : TEXT("-Generate"))};
            failures += Main(artifact_parameters) == 0 ? 0 : 1;
        }
        return failures == 0 ? 0 : 1;
    }
    FString artifact_path;
    if (!FParse::Value(*parameters, TEXT("Artifact="), artifact_path) || artifact_path.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("MaterialSynth requires -Artifact=<file>."));
        print_result(false, validate_only, {}, 1);
        return 1;
    }

    auto const absolute_artifact{FPaths::ConvertRelativePathToFull(artifact_path)};
    TArray<uint8> artifact_bytes;
    if (!FFileHelper::LoadFileToArray(artifact_bytes, *absolute_artifact)) {
        UE_LOG(LogTemp, Error, TEXT("Unable to read compiled material: %s"), *absolute_artifact);
        print_result(false, validate_only, {}, 1);
        return 1;
    }

    auto const compiled{material_synth::deserialize(
        std::span{artifact_bytes.GetData(), static_cast<std::size_t>(artifact_bytes.Num())})};
    if (!compiled) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Unable to load compiled material '%s': %s"),
               *absolute_artifact,
               UTF8_TO_TCHAR(compiled.error().c_str()));
        print_result(false, validate_only, {}, 1);
        return 1;
    }

    auto const& material{compiled->material};
    auto const asset{FString{UTF8_TO_TCHAR(material.settings.package_path.c_str())} + TEXT(".") +
                     UTF8_TO_TCHAR(material.settings.name.c_str())};
    int32 dependency_errors{};
    for (auto const& dependency : material.texture_dependencies) {
        auto const path{texture_object_path(dependency)};
        if (LoadObject<UTexture>(nullptr, *path, nullptr, LOAD_NoWarn) == nullptr) {
            UE_LOG(LogTemp,
                   Error,
                   TEXT("Compiled texture dependency is not a loadable texture: %s"),
                   *path);
            ++dependency_errors;
        }
    }
    if (dependency_errors != 0) {
        print_result(false, validate_only, asset, dependency_errors);
        return 1;
    }
    if (validate_only) {
        print_result(true, true, asset, 0);
        return 0;
    }
    if (GUsingNullRHI) {
        UE_LOG(LogTemp, Error, TEXT("Material generation requires a real rendering RHI."));
        print_result(false, false, asset, 1);
        return 1;
    }

    auto const source_path{FString{UTF8_TO_TCHAR(compiled->source_path.c_str())}};
    auto const source_hash{FString{UTF8_TO_TCHAR(compiled->source_hash.c_str())}};
    auto const emitted{material_synth::emit(material, source_path, source_hash)};
    for (auto const& error : emitted.errors) {
        UE_LOG(LogTemp, Error, TEXT("%s"), *error);
    }
    bool const success{emitted.material != nullptr && emitted.errors.IsEmpty()};
    print_result(success, false, asset, emitted.errors.Num());
    return success ? 0 : 1;
}
