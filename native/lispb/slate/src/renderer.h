#pragma once

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/indices.h>
#include <codegen/sexpr/sources.h>
#include <slate_codegen/document.h>
#include <slate_codegen/indices.h>

#include <string>
#include <string_view>

namespace slate_codegen::detail {

auto lower(std::string_view source_path,
           lispb::codegen::sexpr::Sources const& sources,
           Document const& document,
           DeclarationId declaration,
           lispb::codegen::CppIR& ir) -> lispb::codegen::CppFileId;

}
