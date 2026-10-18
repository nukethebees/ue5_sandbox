#pragma once
#include <codegen/ast/cpp_ir.h>
#include <codegen/schema/manifest.h>
#include <kernel_codegen/document.h>
#include <lispb/compile_mode.h>
#include <lispb/compiled_target.h>
#include <lispb/compiler_storage.h>
#include <lispb/schema/type_graph.h>
#include <lispb/target_id.h>
#include <material_gen/MaterialIR.h>
#include <slate_codegen/document.h>

#include <filesystem>
#include <string>

namespace lispb {
class Project;

class Compiler {
  public:
    Compiler();

    auto storage() -> CompilerStorage& { return storage_; }
    auto cpp() -> codegen::CppIR& { return cpp_; }
    auto manifest() const -> codegen::Manifest const& { return manifest_; }
    auto types() const -> schema::TypeGraph const& { return types_; }

    // Reset semantic, generated and output stores; retain loaded source documents.
    void reset_compilation();
    auto compile(Project const& project,
                 TargetId target,
                 std::filesystem::path const& build_root,
                 CompileMode mode = CompileMode::generate) -> CompiledTarget&;
    auto expand(Project const& project, TargetId target) -> std::string;
  private:
    CompilerStorage storage_;
    codegen::Manifest manifest_;
    schema::TypeGraph types_;
    codegen::CppIR cpp_;
    kernel_codegen::detail::Document kernel_;
    slate_codegen::detail::Document slate_;
    material_synth::MaterialIR material_;
    CompiledTarget output_;
};

} // namespace lispb
