#pragma once

#include "lowered_soa.h"

#include <codegen/schema/registered_type_schema.h>
#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_schema.h>

#include <span>
#include <string_view>

namespace codegen::detail {
enum class SoaReceiver { owner, const_view, mutable_view };
enum class SoaRepresentation { vector, native_vector_view, compact };

auto uses_native_vector_view(SoaSchema const& schema, TypeRegistry const& types) -> bool;

auto logical_column_access(SoaSchema const& schema,
                           std::span<std::string const> path,
                           SoaRepresentation representation,
                           std::string receiver = {},
                           std::map<std::string, SoaSchema const*> const* schemas = nullptr,
                           TypeRegistry const* types = nullptr) -> std::string;
auto lower_soa_api(SoaSchema const& schema,
                   TypeRegistry const& types,
                   SoaRepresentation representation,
                   SoaReceiver receiver,
                   std::string const& type_name,
                   std::map<std::string, SoaSchema const*> const* schemas = nullptr,
                   SoaBackend backend = SoaBackend::unreal,
                   std::string_view equivalent_constructor = {}) -> LoweredSoa;
}
