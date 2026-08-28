#include "lowering_utils.h"
#include "soa_internal.h"

#include <utility>

namespace codegen::detail {
namespace {

TypeDependency const tarray{"TArray", "Containers/Array.h", {}};
TypeDependency const tarray_view{"TArrayView", "Containers/ArrayView.h", {}};

auto composed(std::string spelling, TypeDependency outer, CppType const& contained) -> CppType {
    outer.spelling = spelling;
    outer.dependencies = contained.dependencies;
    return CppType{std::move(spelling), std::vector<TypeDependency>{std::move(outer)}};
}

} // namespace

auto resolve_members(SoaSchema const& schema,
                     std::map<std::string, CppType> const& types)
    -> std::vector<ResolvedMember> {
    std::vector<ResolvedMember> result;
    for (auto const& member : schema.members) {
        auto element{resolve_type(member.type, types)};
        if (member.kind == SoaMemberKind::array) {
            auto container{composed("TArray<" + element.spelling + ">", tarray, element)};
            container.member_operations.emplace(TypeOperation::remove_at_swap, "RemoveAtSwap");
            result.push_back(ResolvedMember{
                .name = member.name,
                .kind = member.kind,
                .element_type = element,
                .container_type = std::move(container),
                .view_type = composed("TArrayView<" + element.spelling + ">", tarray_view, element),
                .const_view_type =
                    composed("TConstArrayView<" + element.spelling + ">", tarray_view, element),
            });
        } else {
            result.push_back(ResolvedMember{
                .name = member.name,
                .kind = member.kind,
                .element_type = element,
                .container_type = element,
                .view_type = CppType{element.spelling + "::View", element.dependencies},
                .const_view_type = CppType{element.spelling + "::ConstView", element.dependencies},
            });
        }
    }
    return result;
}

} // namespace codegen::detail
