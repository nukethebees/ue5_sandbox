#include "jobserver/client.hpp"

#include <Windows.h>

#include <filesystem>
#include <string>

auto main(int argc, char** argv) -> int {
    if (argc != 2 || std::string{argv[1]} != "lease-crash") {
        return 2;
    }

    auto lease{jobserver::Client::acquire({
        .metadata = {.name = "crashing lease client",
                     .kind = "test",
                     .worktree = std::filesystem::current_path()},
        .resources = {{.name = "crash-resource", .mode = jobserver::ClaimMode::exclusive}},
    })};
    if (!lease) {
        return 3;
    }

    TerminateProcess(GetCurrentProcess(), 99);
    return 99;
}
