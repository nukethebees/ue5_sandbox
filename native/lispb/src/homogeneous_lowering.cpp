#include "homogeneous_internal.h"
#include "lowering_utils.h"

namespace codegen::detail {
namespace {

TypeDependency const soa_permutation{"ml::apply_permutation", "SandboxCore/soa_permutation.h", {}};
TypeDependency const tarray_view{"TArrayView", "Containers/ArrayView.h", {}};
TypeDependency const check_dependency{"check", "CoreMinimal.h", {}};

auto homogeneous_permutation_definition(HomogeneousLayoutSchema const& layout,
                                        HomogeneousValueSchema const& value) -> Node {
    NodeListBuilder body;
    body.add(ExpressionStmt{RawExpr{"validate_array_sizes()"}})
        .add(ExpressionStmt{RawExpr{"check(indices.Num() == num())"}, {check_dependency}});
    for (auto const& component : layout.components) {
        body.add(ExpressionStmt{RawExpr{"ml::apply_permutation(" + component + ", indices)"},
                                {soa_permutation}});
    }
    return definition(
        FunctionSpec{
            .name = "apply_permutation",
            .return_type = "void",
            .parameters = {FunctionParameter{CppType{"TArrayView<int32>", {tarray_view}},
                                             "indices"}},
            .body = body.build(),
        },
        "F" + layout.name + value.suffix);
}

auto lower_homogeneous_impl(HomogeneousLayoutSchema const& layout,
                            std::map<std::string, CppType> const& types) -> DeclarationEmission {
    NodeListBuilder header_definitions;
    NodeListBuilder source_definitions;
    bool has_source_definition{};
    header_definitions.append(homogeneous_view_nodes(layout, types));
    for (std::size_t value_index{0}; value_index < layout.value_types.size(); ++value_index) {
        auto const& value{layout.value_types[value_index]};
        header_definitions.new_lines(2).add(homogeneous_storage_node(layout, value, types));

        if (has_source_definition) {
            source_definitions.new_lines(2);
        }
        source_definitions.add(homogeneous_permutation_definition(layout, value));
        has_source_definition = true;
    }
    return {.header = header_definitions.build(), .source = source_definitions.build()};
}

} // namespace

auto lower_homogeneous(HomogeneousLayoutSchema const& schema,
                       std::map<std::string, CppType> const& types) -> DeclarationEmission {
    return lower_homogeneous_impl(schema, types);
}

} // namespace codegen::detail
