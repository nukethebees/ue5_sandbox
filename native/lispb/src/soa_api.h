#pragma once

#include "lowered_soa.h"
#include "lowering_context.h"
#include "soa_receiver.h"

#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>

#include <span>
#include <string>
#include <string_view>

namespace lispb::codegen::detail {

auto logical_column_access(LoweringContext& context,
                           DeclarationHandle schema,
                           std::span<std::string const> path,
                           std::string receiver = {}) -> std::string;
auto lower_soa_api(LoweringContext& context,
                   DeclarationHandle schema,
                   SoaReceiver receiver,
                   std::string const& type_name) -> LoweredSoa;
}
