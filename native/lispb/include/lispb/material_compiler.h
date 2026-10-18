#pragma once

#include <codegen/sexpr/indices.h>

#include <filesystem>

namespace material_synth {
struct MaterialIR;
}

namespace lispb {
struct Compilation;
namespace codegen::sexpr {
class Syntax;
}

void compile_material(codegen::sexpr::Syntax& syntax,
                      codegen::sexpr::SourceFileId source,
                      material_synth::MaterialIR& material,
                      Compilation& compilation,
                      std::filesystem::path const& input,
                      std::filesystem::path const& project_root,
                      std::filesystem::path output,
                      bool emit);

} // namespace lispb
