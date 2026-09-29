#include "server.hpp"

#include "jobserver/transport.hpp"

#include <Windows.h>

auto WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) -> int {
    jobserver::Server server{jobserver::transport::pipe_name()};
    return server.run();
}
