#pragma once

#include <codegen/schema/indices.h>

#include <optional>

namespace lispb::codegen {

struct CppProjection {
    std::optional<TypeRefId> storage;
};

}
