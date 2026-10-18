#include <codegen/complete_dependencies.h>

#include "lispb/schema/indices.h"

#include <span>

namespace lispb::codegen::detail {

auto CompleteDependencies::of(schema::TypeId type) const -> std::span<schema::TypeId const> {
    return std::span{targets}.subspan(offsets[type.value],
                                      offsets[type.value + 1] - offsets[type.value]);
}

}
