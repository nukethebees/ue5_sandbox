#pragma once

#include <codegen/ast/indices.h>
#include <codegen/lowering/indices.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/module_handle.h>
#include <lispb/indices.h>

#include <optional>
#include <string>
#include <string_view>

namespace lispb::schema {
class TypeGraph;
}

namespace lispb::codegen {
class CppIR;
class Declarations;
struct TypeRegistry;
}

namespace lispb::codegen::detail {

struct LoweringContext {
    CppIR& ir;
    Declarations& declarations;
    TypeRegistry const& types;
    schema::TypeGraph const& semantic;
    ModuleHandle module;
    IdentifierId soa_variant_prefix;

    auto path_text(LoweringPathId path, std::string_view separator) const -> std::string;
    auto resolve(TypeRefId reference) const -> CppTypeId;
    auto text(StringId value) const -> std::string;
    auto text(IdentifierId value) const -> std::string;
    auto optional_text(std::optional<StringId> value) const -> std::optional<std::string>;
    auto optional_text(std::optional<IdentifierId> value) const -> std::optional<std::string>;
};

} // namespace lispb::codegen::detail
