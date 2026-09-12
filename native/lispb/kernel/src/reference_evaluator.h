#pragma once

#include "lowering.h"

#include <cstddef>
#include <string>
#include <vector>

namespace kernel_codegen::detail {

struct ReferenceFixture {
    std::vector<std::vector<std::string>> operands;
    std::vector<std::string> expected;
};

auto make_reference_fixture(ExpandedVariant const& expanded, std::size_t count) -> ReferenceFixture;

}
