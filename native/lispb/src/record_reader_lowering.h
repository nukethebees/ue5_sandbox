#pragma once

#include <codegen/schema/declaration_handle.h>

#include <string>
#include <string_view>

namespace lispb::codegen::detail {
struct DeclarationEmission;
struct LoweringContext;

auto reader_header_path(std::string_view header) -> std::string;
void lower_record_reader(LoweringContext& context,
                         DeclarationHandle record,
                         DeclarationEmission& emission,
                         bool overrides = false);
}
