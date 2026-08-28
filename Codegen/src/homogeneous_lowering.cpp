#include "homogeneous_internal.h"
#include "lowering_utils.h"

namespace codegen::detail {
namespace {

TypeDependency const soa_permutation{
    "ml::apply_permutation", "SandboxCore/soa_permutation.h", {}};
TypeDependency const tarray_view{"TArrayView", "Containers/ArrayView.h", {}};
TypeDependency const check_dependency{"check", "CoreMinimal.h", {}};

auto homogeneous_permutation_definition(HomogeneousLayoutSchema const& layout,
                                        HomogeneousValueSchema const& value) -> Node {
    NodeListBuilder body;
    body.add(ExpressionStatement{"validate_array_sizes()"})
        .add(ExpressionStatement{"check(indices.Num() == num())", {check_dependency}});
    for (auto const& component : layout.components) {
        body.add(ExpressionStatement{"ml::apply_permutation(" + component + ", indices)",
                                     {soa_permutation}});
    }
    return definition(
        FunctionSpec{
            .name = "apply_permutation",
            .return_type = "void",
            .parameters = {
                FunctionParameter{CppType{"TArrayView<int32>", {tarray_view}}, "indices"}},
            .body = body.build(),
        },
        "F" + layout.name + value.suffix);
}

auto lower_homogeneous_module_impl(HomogeneousModuleSchema const& module,
                                   std::map<std::string, CppType> const& types) -> Module {
    NodeListBuilder header_nodes;
    header_nodes.add(IncludeDependencies{}, 2);

    NodeListBuilder source_nodes;
    source_nodes.add(Include{source_include(module.settings), false}, 2)
        .add(IncludeDependencies{}, 2);
    for (std::size_t layout_index{0}; layout_index < module.layouts.size(); ++layout_index) {
        auto const& layout{module.layouts[layout_index]};
        header_nodes.add(homogeneous_view_node(layout), 2);
        for (std::size_t value_index{0}; value_index < layout.value_types.size(); ++value_index) {
            auto const& value{layout.value_types[value_index]};
            header_nodes.add(homogeneous_storage_node(layout, value, types));
            if (value_index + 1 < layout.value_types.size()) {
                header_nodes.new_lines(2);
            }

            source_nodes.add(homogeneous_permutation_definition(layout, value));
            if (layout_index + 1 < module.layouts.size() ||
                value_index + 1 < layout.value_types.size()) {
                source_nodes.new_lines(2);
            }
        }
    }
    return Module{
        .name = module.settings.name,
        .header = CppFile{
            .path = module.settings.header,
            .nodes = header_nodes.build(),
            .clang_format_off = true,
            .include_order = module.settings.include_order,
        },
        .source = module.settings.source.has_value()
                      ? std::optional<CppFile>{CppFile{
                            .path = *module.settings.source,
                            .nodes = source_nodes.build(),
                            .pragma_once = false,
                            .clang_format_off = true,
                            .include_order = module.settings.include_order,
                        }}
                      : std::nullopt,
    };
}

} // namespace

auto lower_homogeneous_module(HomogeneousModuleSchema const& module,
                              std::map<std::string, CppType> const& types) -> Module {
    return lower_homogeneous_module_impl(module, types);
}

} // namespace codegen::detail
