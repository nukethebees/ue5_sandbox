#pragma once

#include <ioj/layout/field_override_id.hpp>

#include <cstdint>
#include <map>
#include <string>

namespace ioj::layout {
// Graph-local projection consumed by the layout calculators.
struct VariantOverrides {
    auto operator==(VariantOverrides const&) const -> bool = default;

    std::map<lispb::schema::TypeId, std::string> packed_storage_types;
    std::map<FieldOverrideId, std::uint32_t> packed_field_widths;
    std::map<FieldOverrideId, std::string> soa_column_types;
    std::map<lispb::schema::TypeId, std::uint64_t> capacities;
};
}
