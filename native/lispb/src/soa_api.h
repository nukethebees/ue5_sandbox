#pragma once

#include "lowered_soa.h"
#include "lowering_context.h"
#include "soa_receiver.h"
#include "soa_representation.h"

#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>

#include <map>
#include <span>
#include <string>
#include <string_view>

namespace lispb::codegen::detail {

auto uses_native_vector_view(LoweringContext& context, DeclarationHandle schema) -> bool;

auto logical_column_access(LoweringContext& context,
                           DeclarationHandle schema,
                           std::span<std::string const> path,
                           SoaRepresentation representation,
                           std::string receiver = {},
                           std::map<IdentifierId, DeclarationHandle> const* schemas = nullptr)
    -> std::string;
auto lower_soa_api(LoweringContext& context,
                   DeclarationHandle schema,
                   SoaRepresentation representation,
                   SoaReceiver receiver,
                   std::string const& type_name,
                   std::map<IdentifierId, DeclarationHandle> const* schemas = nullptr,
                   SoaBackend backend = SoaBackend::unreal,
                   StringId equivalent_constructor = {}) -> LoweredSoa;
}
