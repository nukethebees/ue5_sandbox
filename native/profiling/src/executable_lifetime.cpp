#include <tracy/Tracy.hpp>

namespace ioj::profiling {
class ExecutableLifetime {
  public:
    ExecutableLifetime() { tracy::StartupProfiler(); }

    ~ExecutableLifetime() { tracy::ShutdownProfiler(); }
};

// Unreal starts Tracy in SandboxCore; native executables own their own lifetime.
static ExecutableLifetime executable_lifetime;
}
