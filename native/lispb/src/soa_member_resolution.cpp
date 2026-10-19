#include "lowering_context.h"
#include "soa_internal.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/type_operation.h>
#include <codegen/lowering/indices.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>

#include <string>
#include <string_view>
#include <vector>

namespace lispb::codegen::detail {
namespace {
auto composed(LoweringContext& context,
              std::string const& spelling,
              std::string_view const header,
              CppTypeId const contained) -> CppTypeId {
    auto& ir{context.ir};
    std::vector<TypeDependencyId> dependencies{ir.types.dependency(spelling, header)};
    auto const nested{ir.types.dependencies(contained)};
    dependencies.insert(dependencies.end(), nested.begin(), nested.end());
    return ir.types.intern(spelling, dependencies);
}

} // namespace

auto resolve_members(LoweringContext& context, DeclarationHandle schema)
    -> std::vector<ResolvedMemberId> {
    auto& ir{context.ir};
    std::vector<ResolvedMemberId> result;
    auto& resolved{ir.lowering.resolved_members};
    for (auto const member_id : context.declarations.items(
             context.declarations.soas.members[context.declarations.soas.index(schema)])) {
        auto const member{member_id};
        auto element{context.resolve(
            context.declarations.soa_members.type[context.declarations.soa_members.index(member)])};
        if (context.soa_variant_prefix.value != 0 &&
            context.declarations.soa_members.kind[context.declarations.soa_members.index(member)] ==
                SoaMemberKind::nested) {
            // Keep the resolved dependency and policy while selecting this allocator's nested
            // owner.
            auto spelling{std::string{ir.types.spelling(element)}};
            auto const scope{spelling.rfind("::")};
            spelling.insert(scope == std::string::npos ? 0 : scope + 2,
                            context.text(context.soa_variant_prefix));
            element = ir.types.derive(element, spelling);
        }
        if (context.declarations.soa_members.kind[context.declarations.soa_members.index(member)] ==
            SoaMemberKind::array) {
            auto spelling{"TArray<" + std::string{ir.types.spelling(element)}};
            auto const nested{ir.types.dependencies(element)};
            std::vector<TypeDependencyId> dependencies{nested.begin(), nested.end()};
            if (context.declarations.soas
                    .array_allocator[context.declarations.soas.index(schema)]) {
                auto const allocator{context.resolve(
                    *context.declarations.soas
                         .array_allocator[context.declarations.soas.index(schema)])};
                spelling += ", " + std::string{ir.types.spelling(allocator)};
                auto const allocator_dependencies{ir.types.dependencies(allocator)};
                dependencies.insert(dependencies.end(),
                                    allocator_dependencies.begin(),
                                    allocator_dependencies.end());
            }
            spelling += '>';
            dependencies.insert(dependencies.begin(),
                                ir.types.dependency(spelling, "Containers/Array.h"));
            auto const container{ir.types.add(spelling, dependencies)};
            ir.types.set_operation(container, TypeOperation::remove_at_swap, "RemoveAtSwap");
            auto const id{resolved.append()};
            auto const row{resolved.index(id)};
            resolved.source[row] = member;
            resolved.name[row] = context.declarations.soa_members
                                     .name[context.declarations.soa_members.index(member)];
            resolved.kind[row] = context.declarations.soa_members
                                     .kind[context.declarations.soa_members.index(member)];
            resolved.element_type[row] = element;
            resolved.container_type[row] = container;
            resolved.view_type[row] =
                composed(context,
                         "TArrayView<" + std::string{ir.types.spelling(element)} + ">",
                         "Containers/ArrayView.h",
                         element);
            resolved.const_view_type[row] =
                composed(context,
                         "TConstArrayView<" + std::string{ir.types.spelling(element)} + ">",
                         "Containers/ArrayView.h",
                         element);
            result.push_back(id);
        } else {
            auto const id{resolved.append()};
            auto const row{resolved.index(id)};
            resolved.source[row] = member;
            resolved.name[row] = context.declarations.soa_members
                                     .name[context.declarations.soa_members.index(member)];
            resolved.kind[row] = context.declarations.soa_members
                                     .kind[context.declarations.soa_members.index(member)];
            resolved.element_type[row] = element;
            resolved.container_type[row] = element;
            resolved.view_type[row] = ir.types.intern(
                std::string{ir.types.spelling(element)} + "::View", ir.types.dependencies(element));
            resolved.const_view_type[row] =
                ir.types.intern(std::string{ir.types.spelling(element)} + "::ConstView",
                                ir.types.dependencies(element));
            result.push_back(id);
        }
    }
    return result;
}

} // namespace lispb::codegen::detail
