#include <codegen/ast/environment_guard.h>

#include <cassert>

namespace lispb::codegen {
EnvironmentGuard::EnvironmentGuard(EnvironmentId& current, EnvironmentId next)
    : current_{current}
    , previous_{current} {
    assert(!next.is_null());
    current_ = next;
}
EnvironmentGuard::~EnvironmentGuard() {
    current_ = previous_;
}
}
