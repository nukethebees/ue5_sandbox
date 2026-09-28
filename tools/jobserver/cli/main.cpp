#include "broker.hpp"
#include "doctor.hpp"

#include "jobserver/executor.hpp"
#include "jobserver/protocol.hpp"

#include <Windows.h>

#include <algorithm>
#include <iostream>

namespace jobserver::cli {
auto error(Error const& value) -> int {
    std::cerr << "jobserver: " << value.code << ": " << value.message << '\n';
    return 1;
}
auto run(std::vector<std::string> const& args) -> int {
    auto metadata = nlohmann::json{{"name", "command"},
                                   {"kind", "command"},
                                   {"worktree", path_to_utf8(std::filesystem::current_path())}};
    auto cwd{std::filesystem::current_path()};
    std::vector<GateClaim> gates;
    std::size_t index{2};
    for (; index < args.size() && args[index] != "--"; index += 2) {
        if (index + 1 == args.size()) {
            return error({"usage", "Option requires a value"});
        }
        auto const& option{args[index]};
        auto const& value{args[index + 1]};
        if (option == "--shared" || option == "--exclusive") {
            gates.push_back(
                {value, option == "--exclusive" ? LeaseMode::exclusive : LeaseMode::shared});
        } else if (option == "--cwd") {
            cwd = path_from_utf8(value);
        } else if (option == "--name" || option == "--kind" || option == "--task" ||
                   option == "--worktree") {
            metadata[option.substr(2)] = value;
        } else {
            return error({"usage", "Unknown run option " + option});
        }
    }
    if (++index >= args.size()) {
        return error({"usage", "run requires -- executable [arguments]"});
    }
    if (gates.empty()) {
        gates.push_back({"machine", LeaseMode::shared});
    }
    if (GetEnvironmentVariableW(L"NUKETHEBEES_JOBSERVER_MACHINE_MODE", nullptr, 0) != 0 &&
        std::ranges::any_of(gates, [](GateClaim const& gate) { return gate.name == "machine"; })) {
        return error({"nested_machine_gate",
                      "Admit the machine operation once at its outer boundary; inner commands must "
                      "not reacquire machine"});
    }
    Command command{path_from_utf8(args[index++]), {}, cwd};
    command.arguments.assign(args.begin() + static_cast<std::ptrdiff_t>(index), args.end());
    metadata["cwd"] = path_to_utf8(cwd);
    metadata["command_text"] = nlohmann::json(args).dump();
    auto session{Session::connect()};
    if (!session) {
        return error(session.error());
    }
    LocalExecutor executor;
    auto grant{(*session)->acquire(gates, metadata)};
    if (!grant) {
        return error(grant.error());
    }
    auto result{executor.run(command, **session, *grant, gates)};
    auto released{(*session)->release(*grant, result ? *result : 1)};
    if (!result) {
        return error(result.error());
    }
    if (!released) {
        return error(released.error());
    }
    return *result;
}
}
auto wmain(int argc, wchar_t** argv) -> int {
    using namespace jobserver;
    try {
        std::vector<std::string> args;
        for (int i{}; i < argc; ++i) {
            args.push_back(path_to_utf8(argv[i]));
        }
        if (args.size() < 2 || args[1] == "--help") {
            std::cout
                << "jobserver run [--shared gate|--exclusive gate] [--name text] [--kind text] "
                   "[--task text] [--worktree path] [--cwd path] -- executable [arguments]\n"
                   "jobserver broker  (length-prefixed JSON on stdin/stdout)\n"
                   "jobserver status [--json]\njobserver trace [--client N] [--command N] [--lease "
                   "N] [--gate N] [--event kind] [--limit N]\n"
                   "jobserver start|ping|shutdown|doctor|recover --check|recover "
                   "--force|--version\n";
            return 0;
        }
        auto const& command{args[1]};
        if (command == "run") {
            return cli::run(args);
        }
        if (command == "broker") {
            return cli::broker();
        }
        if (command == "status" || command == "trace") {
            auto filters = nlohmann::json::object();
            if (command == "trace") {
                for (std::size_t i{2}; i < args.size(); i += 2) {
                    if (i + 1 == args.size()) {
                        return cli::error({"usage", "Trace filter requires a value"});
                    }
                    auto const key{args[i].substr(2)};
                    if (key == "event") {
                        filters[key] = args[i + 1];
                    } else if (key == "client" || key == "command" || key == "lease" ||
                               key == "gate" || key == "limit") {
                        filters[key] = std::stoull(args[i + 1]);
                    } else {
                        return cli::error({"usage", "Unknown trace filter"});
                    }
                }
            }
            auto result{command == "status" ? Client::status() : Client::trace(filters)};
            if (!result) {
                return cli::error(result.error());
            }
            auto const json = nlohmann::json::parse(*result);
            std::cout << json.dump(args.size() > 2 && args[2] == "--json" ? -1 : 2) << '\n';
            return 0;
        }
        if (command == "version" || command == "--version") {
            std::cout << "jobserver 0.2.0 (protocol " << protocol::major_version << '.'
                      << protocol::minor_version << ")\n";
            return 0;
        }
        if (command == "doctor") {
            bool failed{};
            for (auto const& check : cli::run_doctor()) {
                failed = failed || check.status == cli::DoctorStatus::failure;
                std::cout << (check.status == cli::DoctorStatus::failure   ? "FAIL  "
                              : check.status == cli::DoctorStatus::warning ? "WARN  "
                                                                           : "PASS  ")
                          << check.name << ": " << check.detail << '\n';
            }
            return failed ? 1 : 0;
        }
        if (command == "recover") {
            if (args.size() != 3) {
                return cli::error({"usage", "recover requires --check or --force"});
            }
            if (args[2] == "--check") {
                auto result{Client::check_daemon_recovery()};
                if (!result) {
                    return cli::error(result.error());
                }
                std::cout << "responsive: " << result->responsive
                          << " recoverable: " << result->recoverable
                          << " reason: " << result->reason << '\n';
                return 0;
            }
            if (args[2] != "--force") {
                return cli::error({"usage", "recover requires --check or --force"});
            }
            auto result{Client::force_recover_daemon()};
            if (!result) {
                return cli::error(result.error());
            }
            result = Client::start_daemon();
            return result ? 0 : cli::error(result.error());
        }
        std::expected<void, Error> result;
        if (command == "ping") {
            result = Client::ping();
        } else if (command == "start") {
            result = Client::start_daemon();
        } else if (command == "shutdown") {
            result = Client::shutdown();
        } else {
            return cli::error({"usage", "Unknown command " + command});
        }
        return result ? 0 : cli::error(result.error());
    } catch (std::exception const& exception) {
        return cli::error({"invalid_argument", exception.what()});
    }
}
