#pragma once

#include "SandboxEditor/material/MaterialIR.h"

#include "CoreMinimal.h"

class UMaterial;

namespace material_synth {

inline constexpr TCHAR ownership_key[]{TEXT("MaterialSynth.Owner")};
inline constexpr TCHAR source_key[]{TEXT("MaterialSynth.Source")};
inline constexpr TCHAR source_hash_key[]{TEXT("MaterialSynth.SourceHash")};
inline constexpr TCHAR version_key[]{TEXT("MaterialSynth.Version")};
inline constexpr TCHAR generator_version[]{TEXT("1")};

struct EmitResult {
    UMaterial* material{};
    TArray<FString> errors;
};

SANDBOXEDITOR_API auto emit(MaterialIR const& material,
                            FString const& source_filename,
                            FString const& source_hash) -> EmitResult;

}
