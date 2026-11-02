#pragma once

#include <codegen/ast/environment_id.h>
#include <codegen/ast/indices.h>

namespace lispb::codegen {
struct TypeDestination {
    CppTypeId type{};
    EnvironmentId scope{};
};
}
