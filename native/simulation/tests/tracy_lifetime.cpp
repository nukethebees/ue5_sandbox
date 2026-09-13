#include <tracy/Tracy.hpp>

namespace ml::profiling::tests {
class TracyLifetime final {
  public:
    TracyLifetime() { tracy::StartupProfiler(); }
    ~TracyLifetime() {
        if (TracyIsStarted) {
            tracy::ShutdownProfiler();
        }
    }
};

TracyLifetime tracy_lifetime{};
}
