#pragma once

#include <codegen/schema/soa_backend.h>
#include <lispb/schema/resolved_type_ref.h>
#include <lispb/schema/soa_column.h>
#include <lispb/schema/soa_source_kind.h>

#include <optional>
#include <string>
#include <vector>

namespace lispb::schema {
struct SoaType {
    codegen::SoaBackend backend{codegen::SoaBackend::unreal};
    SoaSourceKind source_kind{SoaSourceKind::structure};
    std::vector<SoaColumn> columns;
    std::optional<ResolvedTypeRef> equivalent_type;
    std::optional<std::string> related_storage_name;
    std::vector<std::string> vector_components;
    bool layout_only{};
    std::optional<std::string> equivalent_constructor{};
};
}
