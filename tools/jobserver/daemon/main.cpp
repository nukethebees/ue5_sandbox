#include "server.hpp"

auto main() -> int {
    jobserver::Server server;
    return server.run();
}
