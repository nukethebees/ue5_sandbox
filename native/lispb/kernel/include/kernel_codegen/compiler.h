#pragma once

#include <codegen/sexpr/indices.h>
#include <lispb/kernel_profile.h>

#include <span>

namespace lispb {
struct Compilation;
}
namespace lispb::codegen::sexpr {
class Syntax;
}
namespace kernel_codegen::detail {
struct Document;
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
             lispb::KernelProfile profile,
             bool generate);

}
