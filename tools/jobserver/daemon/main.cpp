#include "server.hpp"

#include <Windows.h>

auto WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) -> int {
    jobserver::Server server;
    return server.run();
}
