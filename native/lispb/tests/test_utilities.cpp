#include "test_utilities.h"

#include "compiler_test_context.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/generated_file.h>
#include <codegen/generator.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/type_registry.h>
#include <codegen/source_loader.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen {

auto make_test_registry(CompilerTestContext& context,
                        std::vector<std::pair<std::string_view, CppTypeId>> const& entries)
    -> TypeRegistry {
    TypeRegistry types{context.storage.registered_types};
    for (auto const& [name, type] : entries) {
        types.add(name, type);
    }
    return types;
}
auto lower_test_manifest(CompilerTestContext& context, Manifest manifest) -> std::vector<ModuleId> {
    auto const graph{schema::resolve_type_graph(manifest)};
    return lower_modules(context.ir, manifest, graph);
}
auto parse_test_manifest(CompilerTestContext& context,
                         std::string_view source,
                         std::vector<std::pair<std::string_view, CppTypeId>> const& registrations)
    -> Manifest {
    auto& storage{context.storage};
    auto const file{storage.sources.add("test.lispb", source)};
    return {manifest_schema_version,
            make_test_registry(context, registrations),
            storage.declarations,
            parse_module_source(storage, file).modules};
}
auto find_test_type(schema::TypeGraph const& graph, std::string_view module, std::string_view name)
    -> std::optional<schema::TypeId> {
    auto const module_name{graph.strings().find(module)};
    auto const type_name{graph.strings().find(name)};
    for (auto const node : graph.types()) {
        if (graph.module_name(node) == module_name && graph.name(node) == type_name) {
            return node;
        }
    }
    return std::nullopt;
}
auto render_test_manifest(CompilerTestContext& context, Manifest manifest)
    -> std::vector<GeneratedFile> {
    return render_modules(context.ir, lower_test_manifest(context, std::move(manifest)));
}
auto collected_dependencies(CppIR const& ir, NodeId node) -> std::vector<TypeDependencyId> {
    std::vector<TypeDependencyId> result;
    ir.collect_dependencies(node, result);
    return result;
}
auto collected_dependencies(CppIR const& ir, Expr expression) -> std::vector<TypeDependencyId> {
    std::vector<TypeDependencyId> result;
    ir.collect_dependencies(expression, result);
    return result;
}
auto occurrences(std::string const& text, std::string const& value) -> std::size_t {
    std::size_t count{};
    auto position{text.find(value)};
    while (position != std::string::npos) {
        ++count;
        position = text.find(value, position + value.size());
    }
    return count;
}

}
