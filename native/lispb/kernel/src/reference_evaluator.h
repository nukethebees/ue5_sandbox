#pragma once

#include "lowering.h"

#include <kernel_codegen/constant_kind.h>

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

namespace kernel_codegen::detail {

using ReferenceValue = std::variant<std::string, ConstantKind>;

struct ReferenceFixture {
    std::vector<std::vector<ReferenceValue>> operands;
    std::vector<ReferenceValue> expected;
};

auto make_reference_fixture(Document const& document, ExpandedVariantId const id, std::size_t count)
    -> ReferenceFixture;

}
