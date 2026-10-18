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

auto make_reference_fixture(Document const& document, ExpandedVariantId const id, std::size_t count)
    -> ReferenceFixture;

}
