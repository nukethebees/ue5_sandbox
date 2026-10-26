#pragma once

#include <ioj/layout/resolved_variant.hpp>

#include <lispb/schema/bit_layout.h>
#include <lispb/schema/type_graph.h>

#include <cstdint>
#include <string>

namespace ioj::layout::analysis_detail {

auto effective_bit_layout(lispb::schema::TypeGraph const& types,
                          lispb::schema::TypeId type,
                          ResolvedVariant const& variant) -> lispb::schema::BitLayout;

auto effective_storage_type(lispb::schema::TypeGraph const& types,
                            lispb::schema::TypeId const type,
                            lispb::schema::TypeDefinitionId const& packed,
                            ResolvedVariant const& variant) -> std::string;

auto effective_column_type(lispb::schema::TypeGraph const& types,
                           lispb::schema::TypeId const type,
                           lispb::schema::SoaColumnId const& column,
                           ResolvedVariant const& variant) -> std::string;

}
