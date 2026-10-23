#include "enum_backing.hpp"

#include "integer_code_domain.hpp"

#include <lispb/schema/enum_domains.h>
#include <lispb/schema/enum_storage_requirement.h>
#include <lispb/schema/enum_types.h>

#include <cstdint>
#include <optional>
#include <string>

namespace ioj::layout::analysis_detail {

auto enum_domain_fits_backing(EnumCodeValue const minimum,
                              EnumCodeValue const maximum,
                              PhysicalFacts const& facts,
                              std::uint64_t const backing_bits) -> std::optional<bool> {
    if (!facts.integer_signed.has_value()) {
        return std::nullopt;
    }
    if (!*facts.integer_signed) {
        if (!facts.unsigned_value_bits.has_value()) {
            return std::nullopt;
        }
        auto const maximum_value{maximum_unsigned_value(*facts.unsigned_value_bits)};
        return maximum_value.has_value() && !minimum.negative && !maximum.negative &&
               maximum.magnitude <= *maximum_value;
    }
    if (backing_bits == 0 || backing_bits > 64) {
        return std::nullopt;
    }
    auto const negative_magnitude_limit{
        backing_bits == 64 ? (std::uint64_t{1} << 63) : (std::uint64_t{1} << (backing_bits - 1))};
    auto const positive_limit{negative_magnitude_limit - 1};
    auto const minimum_fits{!minimum.negative || minimum.magnitude <= negative_magnitude_limit};
    auto const maximum_fits{maximum.negative || maximum.magnitude <= positive_limit};
    return minimum_fits && maximum_fits;
}

auto derived_enum_backing_type(lispb::schema::TypeGraph const& types,
                               lispb::schema::TypeDefinitionId const& enumeration)
    -> std::optional<std::string> {
    auto const& domains{types.enum_domains()};
    auto const domain{
        types.definitions().enums.domain[types.definitions().enums.index(enumeration)]};
    auto const requirement{lispb::schema::derive_enum_storage_requirement(
        domains,
        domain,
        types.definitions().enums.bit_width[types.definitions().enums.index(enumeration)])};
    if (!requirement.has_value()) {
        return std::nullopt;
    }
    return "std::" + std::string{requirement->signedness ? "int" : "uint"} +
           std::to_string(requirement->bit_width) + "_t";
}

}
