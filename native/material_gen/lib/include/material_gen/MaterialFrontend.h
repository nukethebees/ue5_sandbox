#pragma once

#include <material_gen/MaterialIR.h>

#include <optional>
#include <string_view>

namespace material_synth {

using TextureResolveFunction = std::optional<std::string> (*)(void const* context,
                                                              std::string_view texture_asset_path);

struct TextureResolver {
    void const* context{};
    TextureResolveFunction resolve{};
};

struct AnalysisResult {
    std::optional<MaterialIR> material;
    std::vector<Diagnostic> diagnostics;
};

auto analyze(std::string_view path, std::string_view source, TextureResolver texture_resolver)
    -> AnalysisResult;

}
