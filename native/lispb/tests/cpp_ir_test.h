#pragma once

#include "compiler_test_context.h"

#include <codegen/ast/cpp_renderer.h>

#include <gtest/gtest.h>

namespace lispb::codegen {

class CppIrTest : public testing::Test {
  protected:
    CompilerTestContext context;
    CppIR& ir{context.ir};
    CppTypes& types{ir.types};
    CppRenderer renderer{ir};
};

using AstTest = CppIrTest;
using ExprTest = CppIrTest;

}
