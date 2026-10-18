#pragma once

#include <ioj/layout/variant_id.hpp>
#include <ioj/layout/variant_overrides.hpp>

#include <string>

namespace ioj::layout {

// Graph-local projection of one durable variant for the layout calculators.
struct ResolvedVariant {
    VariantId id{};
    std::string name;
    VariantOverrides overrides;
};

}
