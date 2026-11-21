#pragma once

#include <codegen/sexpr/indices.h>

#include <filesystem>
#include <stdexcept>

namespace material_synth {
struct MaterialIR;
}

namespace lispb {
class MaterialSourceError final : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
struct Compilation;
namespace codegen::sexpr {
class Syntax;
}

void compile_material(codegen::sexpr::Syntax& syntax,
                      codegen::sexpr::SourceFileId source,
                      material_synth::MaterialIR& material,
                      Compilation& result,
                      std::filesystem::path const& input,
                      std::filesystem::path const& project_root,
                      std::filesystem::path output,
                      bool emit);

} // namespace lispb
