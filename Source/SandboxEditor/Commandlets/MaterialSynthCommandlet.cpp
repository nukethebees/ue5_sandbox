#include "SandboxEditor/Commandlets/MaterialSynthCommandlet.h"

#include "SandboxEditor/material/MaterialEmitter.h"
#include "SandboxEditor/material/MaterialFrontend.h"
#include "SandboxEditor/material/MaterialSourceHash.h"

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
        UE_LOG(LogTemp, Error, TEXT("MaterialSynth -GenerateAll is not implemented."));
        print_result(false, validate_only, {}, 1);
        return 1;
    }
    FString input;
    if (!FParse::Value(*parameters, TEXT("Input="), input) || input.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("MaterialSynth requires -Input=<file>."));
        print_result(false, validate_only, {}, 1);
        return 1;
    }

    auto const absolute_input{FPaths::ConvertRelativePathToFull(input)};
    FString source;
    TArray<uint8> source_bytes;
    if (!FFileHelper::LoadFileToString(source, *absolute_input) ||
        !FFileHelper::LoadFileToArray(source_bytes, *absolute_input)) {
        UE_LOG(LogTemp, Error, TEXT("Unable to read material source: %s"), *absolute_input);
        print_result(false, validate_only, {}, 1);
        return 1;
    }

    auto const utf8_source{StringCast<UTF8CHAR>(*source)};
    auto const utf8_path{StringCast<UTF8CHAR>(*absolute_input)};
    auto const analysis{material_synth::analyze(
        reinterpret_cast<char const*>(utf8_path.Get()),
        std::string_view{reinterpret_cast<char const*>(utf8_source.Get()),
                         static_cast<std::size_t>(utf8_source.Length())},
        [](std::string_view const requested) -> std::optional<std::string> {
            auto const path{texture_object_path(requested)};
            auto* const texture{LoadObject<UTexture>(nullptr, *path, nullptr, LOAD_NoWarn)};
            if (texture == nullptr) {
                return std::nullopt;
            }
            auto const resolved{StringCast<UTF8CHAR>(*texture->GetPathName())};
            return std::string{reinterpret_cast<char const*>(resolved.Get()),
                               static_cast<std::size_t>(resolved.Length())};
        })};

    FString asset;
    if (analysis.material) {
        asset = FString{UTF8_TO_TCHAR(analysis.material->settings.package_path.c_str())} +
                TEXT(".") + UTF8_TO_TCHAR(analysis.material->settings.name.c_str());
    }
    for (auto const& diagnostic : analysis.diagnostics) {
        UE_LOG(LogTemp,
               Error,
               TEXT("%s:%llu:%llu: %s"),
               UTF8_TO_TCHAR(diagnostic.path.c_str()),
               diagnostic.line,
               diagnostic.column,
               UTF8_TO_TCHAR(diagnostic.message.c_str()));
    }
    if (!analysis.material) {
        print_result(false, validate_only, asset, static_cast<int32>(analysis.diagnostics.size()));
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

    auto const source_hash{material_synth::sha256(source_bytes)};
    auto relative_input{absolute_input};
    FPaths::MakePathRelativeTo(relative_input, *FPaths::ProjectDir());

    auto const emitted{material_synth::emit(*analysis.material, relative_input, source_hash)};
    for (auto const& error : emitted.errors) {
        UE_LOG(LogTemp, Error, TEXT("%s"), *error);
    }
    bool const success{emitted.material != nullptr && emitted.errors.IsEmpty()};
    print_result(success, false, asset, emitted.errors.Num());
    return success ? 0 : 1;
}
