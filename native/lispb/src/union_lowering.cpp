#include "lowering.h"
#include "lowering_utils.h"

#include <map>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace codegen::detail {
namespace {

auto alternative_type(UnionAlternativeSchema const& alternative, TypeRegistry const& types)
    -> CppType {
    auto element{resolve_type(alternative.type, types)};
    if (!alternative.count.has_value()) {
        return element;
    }
    TypeDependency array_dependency{"std::array", "array", std::move(element.dependencies)};
    return CppType{"std::array<" + element.spelling + ", " + std::to_string(*alternative.count) +
                       ">",
                   {std::move(array_dependency)}};
}

auto union_node(UnionSchema const& schema, TypeRegistry const& types) -> Node {
    NodeListBuilder alternatives;
    for (auto const& alternative : schema.alternatives) {
        alternatives.add(Member{alternative_type(alternative, types), alternative.name});
    }
    return Struct{.name = schema.name,
                  .children = alternatives.build(),
                  .export_specifier = schema.export_specifier,
                  .record_kind = "union"};
}

auto tagged_union_node(TaggedUnionSchema const& schema, TypeRegistry const& types) -> Node {
    NodeListBuilder alternatives;
    for (auto const& alternative : schema.alternatives) {
        alternatives.add(LineComment{"tag: " + alternative.tag});
        alternatives.add(Member{alternative_type(UnionAlternativeSchema{.name = alternative.name,
                                                                        .type = alternative.type,
                                                                        .count = alternative.count},
                                                 types),
                                alternative.name});
    }

    NodeListBuilder members;
    members.add(Member{resolve_type(schema.discriminant, types), "tag"});
    members.add(
        Struct{.name = "Payload", .children = alternatives.build(), .record_kind = "union"});
    members.add(Member{CppType{"Payload"}, "payload"});
    return Struct{.name = schema.name,
                  .children = members.build(),
                  .export_specifier = schema.export_specifier};
}

} // namespace

auto lower_union(UnionSchema const& schema, TypeRegistry const& types) -> DeclarationEmission {
    return {.header = {union_node(schema, types)}};
}

auto lower_tagged_union(TaggedUnionSchema const& schema, TypeRegistry const& types)
    -> DeclarationEmission {
    return {.header = {tagged_union_node(schema, types)}};
}

} // namespace codegen::detail
