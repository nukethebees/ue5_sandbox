#pragma once

#include <codegen/ast/access.h>
#include <codegen/ast/record_kind.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>

namespace lispb::codegen {

struct FacadeSchemas {
    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    std::vector<IdentifierId> name;
    std::vector<TypeRefId> target_type;
    std::vector<IdentifierId> target_member_name;
    std::vector<SchemaRange<FacadeMethodId>> methods;
    std::vector<SchemaRange<StringId>> validation_lines;
    std::vector<SchemaRange<IdentifierId>> validation_dependencies;
    std::vector<IdentifierId> export_specifier;
    std::vector<Access> bind_access;
    std::vector<Access> method_access;
    std::vector<SchemaRange<StringId>> friends;
    std::vector<RecordKind> friend_kind;
    std::vector<std::uint8_t> definitions_in_source;
    std::vector<std::uint8_t> reference_target;
};

}
