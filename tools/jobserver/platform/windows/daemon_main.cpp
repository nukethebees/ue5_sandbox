#include "jobserver/platform/transport.hpp"
#include "jobserver/server/server.hpp"

#include <Windows.h>

auto WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) -> int {
    jobserver::server::Server server{jobserver::platform::default_endpoint()};
    return server.run();
}
