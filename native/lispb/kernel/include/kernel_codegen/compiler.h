#pragma once

#include <codegen/sexpr/indices.h>

#include <span>
#include <string_view>

namespace lispb {
struct Compilation;
}
namespace lispb::codegen::sexpr {
class Syntax;
}
namespace kernel_codegen::detail {
struct Document;
auto is_identifier(std::string_view value) -> bool;
}

namespace lispb::codegen {
class CppIR;
}

namespace kernel_codegen {

void compile(lispb::codegen::sexpr::Syntax& syntax,
             detail::Document& document,
             lispb::codegen::CppIR& cpp,
             lispb::Compilation& output,
             std::span<lispb::codegen::sexpr::SourceFileId const> inputs,
             bool generate);

}
