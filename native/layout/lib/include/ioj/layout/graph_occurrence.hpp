#pragma once

#include <lispb/schema/indices.h>

namespace ioj::layout {
struct GraphOccurrence {
    lispb::schema::TypeId type;
    bool boundary{};
};
}
