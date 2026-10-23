#pragma once
#include <codegen/ast/indices.h>
#include <codegen/lowering/indices.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/soa_member_kind.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>
namespace lispb::codegen {
struct ResolvedMembers {
    auto append() -> ResolvedMemberId;
    auto index(ResolvedMemberId id) const -> std::uint32_t;
    void clear();
    std::vector<SoaMemberId> source;
    std::vector<StringId> name;
    std::vector<SoaMemberKind> kind;
    std::vector<CppTypeId> element_type;
    std::vector<CppTypeId> container_type;
    std::vector<CppTypeId> view_type;
    std::vector<CppTypeId> const_view_type;
};
}
