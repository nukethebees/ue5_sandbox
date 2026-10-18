#pragma once

#include <ioj/layout/analyzer/enum_code_value.hpp>
#include <ioj/layout/type_facts.hpp>

#include <lispb/schema/type_graph.h>

#include <cstdint>
#include <optional>
#include <string>

namespace ioj::layout::analysis_detail {

auto enum_domain_fits_backing(EnumCodeValue const minimum,
                              EnumCodeValue const maximum,
                              TypeFacts const& facts,
                              std::uint64_t const backing_bits) -> std::optional<bool>;

auto derived_enum_backing_type(lispb::schema::TypeGraph const& types,
                               lispb::schema::TypeDefinitionId const& enumeration)
    -> std::optional<std::string>;

}
