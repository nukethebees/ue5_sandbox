#pragma once
#include <codegen/ast/indices.h>
#include <codegen/generated_file.h>

#include <filesystem>
#include <span>
#include <vector>
namespace lispb {
struct CompilerStorage;
struct Compilation;
namespace schema {
class TypeGraph;
}
}
namespace lispb::codegen {
class CppIR;
struct Manifest;
auto lower_modules(CppIR& ir, Manifest& input, schema::TypeGraph const& graph)
    -> std::vector<ModuleId>;
auto render_modules(CppIR& ir, std::span<ModuleId const> modules) -> std::vector<GeneratedFile>;
void compile_schema(CompilerStorage& storage,
                    CppIR& ir,
                    Manifest& manifest,
                    schema::TypeGraph& graph,
                    Compilation& output,
                    std::filesystem::path const& types,
                    std::span<std::filesystem::path const> modules,
                    bool emit);
auto generate_files(std::vector<GeneratedFile> const& files,
                    std::filesystem::path const& project_root,
                    std::filesystem::path const& output_root,
                    bool check_only) -> int;
}
