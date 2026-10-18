#pragma once

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/schema/declarations.h>
#include <lispb/compiler_storage.h>
#include <lispb/strings.h>

namespace lispb::codegen {

struct CompilerTestContext {
    CompilerStorage storage;
    CppIR ir{storage.strings, storage.cpp_types};
};

}
