#include <tracy/Tracy.hpp>

namespace ioj::sim::tests::profiling {
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
