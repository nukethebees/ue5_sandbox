#pragma once
#include <codegen/ast/environment_id.h>

namespace lispb::codegen {
// Select one environment for a lexical emission region, then restore its caller's environment.
class EnvironmentGuard {
  public:
    EnvironmentGuard(EnvironmentId& current, EnvironmentId next);
    ~EnvironmentGuard();
    EnvironmentGuard(EnvironmentGuard const&) = delete;
    auto operator=(EnvironmentGuard const&) -> EnvironmentGuard& = delete;
  private:
    EnvironmentId& current_;
    EnvironmentId previous_;
};
}
