#pragma once

#include <codegen/schema/declaration_handle.h>

#include <string>
#include <string_view>

namespace lispb::codegen::detail {
struct LoweringContext;
struct DeclarationEmission;

auto writer_header_path(std::string_view header) -> std::string;
void lower_record_writer(LoweringContext& ctx,
                         DeclarationHandle record,
                         DeclarationEmission& emission);
}
