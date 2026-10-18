#pragma once

#include <codegen/sexpr/indices.h>

#include <vector>

namespace lispb::codegen::sexpr {
class Syntax;
}

namespace material_synth {
struct Diagnostic;
struct MaterialIR;
struct TextureResolver;
auto analyze(lispb::codegen::sexpr::Syntax& syntax,
             lispb::codegen::sexpr::SourceFileId source,
             TextureResolver texture_resolver,
             MaterialIR& material) -> std::vector<Diagnostic>;

}
