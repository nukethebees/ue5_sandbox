#pragma once

#include "compiler_test_context.h"

#include <codegen/ast/cpp_renderer.h>

#include <gtest/gtest.h>

#include <source_location>
#include <string_view>

namespace lispb::codegen {

class CppIrTest : public testing::Test {
  protected:
    void expect_rendered(auto const& value,
                         std::string_view const expected,
                         std::source_location const location = std::source_location::current()) {
        ::testing::ScopedTrace const trace{
            location.file_name(), static_cast<int>(location.line()), "rendered C++"};
        EXPECT_EQ(renderer.render(value), expected);
    }

    CompilerTestContext context;
    CppIR& ir{context.ir};
    CppTypes& types{ir.types};
    CppRenderer renderer{ir};
};

using AstTest = CppIrTest;
using ExprTest = CppIrTest;

}
