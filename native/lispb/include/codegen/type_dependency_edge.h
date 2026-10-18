#pragma once

#include <lispb/schema/indices.h>

namespace lispb::codegen::detail {

struct TypeDependencyEdge {
    schema::TypeId owner;
    schema::TypeId target;
};

}
