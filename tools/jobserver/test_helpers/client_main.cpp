#include "jobserver/client.hpp"

#include <Windows.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

auto main(int argc, char** argv) -> int {
    if (argc < 2) {
        return 2;
    }
    auto const mode{std::string{argv[1]}};
    auto const crash{mode == "lease-crash"};
    auto const hold{mode == "lease-hold" && argc == 5};
    auto const detached_run{mode == "detached-run" && argc == 3};
    auto const attached_run{mode == "attached-run" && argc == 3};
    if (mode == "recover" && argc == 2) {
        return jobserver::Client::force_recover_daemon() ? 0 : 3;
    }
    if (detached_run || attached_run) {
        auto const result{jobserver::Client::run(
            {
                .metadata = {.name = detached_run ? "detached client job" : "attached client job",
                             .kind = "test",
                             .worktree = std::filesystem::current_path()},
                .command = {.executable = JOBSERVER_TEST_HELPER_PATH,
                            .arguments = {"marker-after", argv[2], "800"},
                            .working_directory =
                                std::filesystem::path{JOBSERVER_TEST_HELPER_PATH}.parent_path(),
                            .environment = {}},
                .resources = {},
                .timeout = std::nullopt,
                .suspect_after = std::nullopt,
                .disconnect_policy = detached_run ? jobserver::DisconnectPolicy::continue_job
                                                  : jobserver::DisconnectPolicy::cancel,
            },
            [](std::string const&, std::string const&) {})};
        return result ? *result : 3;
    }
    if (!crash && !hold) {
        return 2;
    }
    auto const name{hold ? argv[2] : "crashing lease client"};
    auto const resource{hold ? argv[3] : "crash-resource"};
    auto lease{jobserver::Client::acquire({
        .metadata = {.name = name, .kind = "test", .worktree = std::filesystem::current_path()},
        .resources = {{.name = resource, .mode = jobserver::ClaimMode::exclusive}},
    })};
    if (!lease) {
        return 3;
    }

    if (crash) {
        TerminateProcess(GetCurrentProcess(), 99);
        return 99;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{std::atoi(argv[4])});
    return 0;
}
