#pragma once

#include <string>

namespace lispb {
struct Compilation;
}
namespace lispb::codegen {
class CppIR;
}
namespace lispb::codegen::sexpr {
class Syntax;
}
namespace slate_codegen::detail {
struct Document;
}

namespace slate_codegen {

struct SourceOptions;

void compile_sources(lispb::codegen::sexpr::Syntax& syntax,
                     detail::Document& document,
                     lispb::codegen::CppIR& ir,
                     lispb::Compilation& result,
                     SourceOptions const& options,
                     bool emit);
auto expand_sources(lispb::codegen::sexpr::Syntax& syntax, SourceOptions const& options)
    -> std::string;

}
