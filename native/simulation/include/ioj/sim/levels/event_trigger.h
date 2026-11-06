#pragma once
#include <ioj/sim/expressions/expr_handle.h>
#include <ioj/sim/index_span.h>

namespace ioj::sim {
struct EventTrigger {
    ExprHandle condition;
    IndexSpan events;
};
}
