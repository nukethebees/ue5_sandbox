#include "doctor.hpp"

#include "jobserver/client.hpp"
#include "jobserver/protocol.hpp"

#include <Windows.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
using Json = nlohmann::json;

auto narrow(std::wstring_view const text) -> std::string {
    if (text.empty()) {
        return {};
    }
    auto const size{WideCharToMultiByte(CP_UTF8,
                                        WC_ERR_INVALID_CHARS,
                                        text.data(),
                                        static_cast<int>(text.size()),
                                        nullptr,
                                        0,
                                        nullptr,
                                        nullptr)};
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8,
                        WC_ERR_INVALID_CHARS,
                        text.data(),
                        static_cast<int>(text.size()),
                        result.data(),
                        size,
                        nullptr,
                        nullptr);
    return result;
}

void print_help() {
    std::cout << "NukeTheBees local job scheduler\n\n"
                 "  jobserver run [options] -- <command> [arguments...]\n"
                 "  jobserver status [--json]\n"
                 "  jobserver show <job-id>\n"
                 "  jobserver history [--json]\n"
                 "  jobserver cancel <job-id>\n"
                 "  jobserver kill <job-id>\n"
                 "  jobserver logs <job-id>\n"
                 "  jobserver start\n"
                 "  jobserver ping\n"
                 "  jobserver shutdown\n"
                 "  jobserver recover --check|--force\n"
                 "  jobserver doctor\n"
                 "  jobserver version\n\n"
                 "run options:\n"
                 "  --name <name> --kind <kind> --worktree <path>\n"
                 "  --resource <name=units> --shared <name> --exclusive <name>\n"
                 "  --timeout <Nms|Ns|Nm> --suspect-after <Nms|Ns|Nm> --detach\n";
}

auto parse_duration(std::string const& text) -> std::chrono::milliseconds {
    auto multiplier{1000LL};
    auto digits{text};
    if (text.ends_with("ms")) {
        multiplier = 1;
        digits.resize(text.size() - 2);
    } else if (text.ends_with('m')) {
        multiplier = 60'000;
        digits.resize(text.size() - 1);
    } else if (text.ends_with('s')) {
        digits.resize(text.size() - 1);
    }
    return std::chrono::milliseconds{std::stoll(digits) * multiplier};
}

auto resolve_executable(std::filesystem::path const& executable) -> std::filesystem::path {
    if (executable.has_parent_path()) {
        return executable;
    }

    auto const required{SearchPathW(nullptr, executable.c_str(), L".exe", 0, nullptr, nullptr)};
    if (required == 0) {
        return executable;
    }
    std::vector<wchar_t> buffer(static_cast<std::size_t>(required) + 1);
    auto const written{SearchPathW(nullptr,
                                   executable.c_str(),
                                   L".exe",
                                   static_cast<DWORD>(buffer.size()),
                                   buffer.data(),
                                   nullptr)};
    if (written == 0 || written >= buffer.size()) {
        return executable;
    }
    return std::filesystem::path{buffer.data()};
}

auto print_error(jobserver::Error const& error) -> int {
    std::cerr << "jobserver: " << error.message << " [" << error.code << "]\n";
    return 125;
}

auto local_app_data() -> std::filesystem::path {
    char* value{};
    std::size_t size{};
    if (_dupenv_s(&value, &size, "LOCALAPPDATA") != 0 || value == nullptr) {
        return {};
    }
    std::filesystem::path result{value};
    std::free(value);
    return result;
}

void print_status(std::string const& text) {
    auto const status = Json::parse(text, nullptr, false);
    if (!status.is_object()) {
        std::cout << text << '\n';
        return;
    }
    auto const daemon = status.value("daemon", Json::object());
    if (!daemon.empty()) {
        std::cout << "DAEMON\n";
        std::cout << "  pid " << daemon.value("process_id", 0U) << "  uptime "
                  << daemon.value("uptime_ms", 0LL) << "ms  audit "
                  << daemon.value("last_audit_age_ms", 0LL) << "ms ago  job handlers "
                  << daemon.value("job_handlers", daemon.value("active_handlers", 0U)) << '/'
                  << daemon.value("handler_capacity", 0U) << "  control handlers "
                  << daemon.value("control_handlers", 0U) << '/'
                  << daemon.value("control_handler_capacity", 0U) << "  rejected "
                  << daemon.value("rejected_clients", 0ULL) << "  supervisors "
                  << daemon.value("supervised_jobs", 0U) << "  leases "
                  << daemon.value("leases", 0U) << '\n';
    }
    std::cout << "JOBS\n";
    for (auto const& job : status.value("jobs", Json::array())) {
        std::cout << "  " << job.value("id", "?") << "  " << job.value("state", "UNKNOWN") << "  "
                  << job.value("kind", "") << "  " << job.value("name", "");
        auto const blockers{job.value("blockers", std::vector<std::string>{})};
        for (auto const& claim : job.value("claims", Json::array())) {
            std::cout << "  " << claim.value("name", "") << ':' << claim.value("mode", "") << '='
                      << claim.value("units", 1U);
        }
        if (!blockers.empty()) {
            std::cout << "  waiting on:";
            for (auto const& blocker : blockers) {
                std::cout << ' ' << blocker;
            }
        }
        auto const running_ms{job.value("running_ms", 0LL)};
        if (running_ms != 0) {
            std::cout << "  " << running_ms << "ms";
        }
        if (job.value("health", "NORMAL") != "NORMAL") {
            std::cout << "  " << job.value("health_reason", "");
        }
        std::cout << '\n';
        auto const worktree{job.value("worktree", "")};
        auto const submit_directory{job.value("submit_directory", "")};
        std::cout << "      worktree: " << (worktree.empty() ? "<unspecified>" : worktree) << '\n';
        if (!submit_directory.empty() && submit_directory != worktree) {
            std::cout << "      submitted-from: " << submit_directory << '\n';
        }
    }
    std::cout << "RESOURCES\n";
    for (auto const& resource : status.value("resources", Json::array())) {
        std::cout << "  " << resource.value("name", "") << "  " << resource.value("used", 0U) << '/'
                  << resource.value("capacity", 0U);
        if (resource.value("exclusive", false)) {
            std::cout << " exclusive";
        }
        std::cout << '\n';
    }
    auto const diagnostics{status.value("diagnostics", std::vector<std::string>{})};
    if (!diagnostics.empty()) {
        std::cout << "RECOVERIES\n";
        for (auto const& diagnostic : diagnostics) {
            std::cout << "  " << diagnostic << '\n';
        }
    }
}

auto require_value(std::vector<std::string> const& arguments, std::size_t& index)
    -> std::string const* {
    ++index;
    return index < arguments.size() ? &arguments[index] : nullptr;
}

auto run_command(std::vector<std::string> const& arguments) -> int {
    jobserver::SubmitRequest request{
        .metadata = {.name = "command",
                     .kind = "command",
                     .worktree = std::filesystem::current_path()},
        .command = {.executable = {},
                    .arguments = {},
                    .working_directory = std::filesystem::current_path(),
                    .environment = {}},
        .resources = {},
        .timeout = std::nullopt,
        .suspect_after = std::nullopt,
        .disconnect_policy = jobserver::DisconnectPolicy::cancel,
    };
    std::size_t index{1};
    for (; index < arguments.size(); ++index) {
        auto const& argument{arguments[index]};
        if (argument == "--") {
            ++index;
            break;
        }
        if (argument == "--detach") {
            request.disconnect_policy = jobserver::DisconnectPolicy::continue_job;
            continue;
        }
        auto const* value{require_value(arguments, index)};
        if (value == nullptr) {
            std::cerr << "jobserver: " << argument << " requires a value\n";
            return 2;
        }
        if (argument == "--name") {
            request.metadata.name = *value;
        } else if (argument == "--kind") {
            request.metadata.kind = *value;
        } else if (argument == "--worktree") {
            request.metadata.worktree = jobserver::path_from_utf8(*value);
        } else if (argument == "--shared") {
            request.resources.push_back({.name = *value, .mode = jobserver::ClaimMode::shared});
        } else if (argument == "--exclusive") {
            request.resources.push_back({.name = *value, .mode = jobserver::ClaimMode::exclusive});
        } else if (argument == "--timeout") {
            request.timeout = parse_duration(*value);
        } else if (argument == "--suspect-after") {
            request.suspect_after = parse_duration(*value);
        } else if (argument == "--resource") {
            auto const separator{value->find('=')};
            if (separator == std::string::npos) {
                std::cerr << "jobserver: counted resources use name=units\n";
                return 2;
            }
            request.resources.push_back({
                .name = value->substr(0, separator),
                .mode = jobserver::ClaimMode::counted,
                .units = static_cast<std::uint32_t>(std::stoul(value->substr(separator + 1))),
            });
        } else {
            std::cerr << "jobserver: unknown run option " << argument << '\n';
            return 2;
        }
    }
    if (index >= arguments.size()) {
        std::cerr << "jobserver: run requires a command after --\n";
        return 2;
    }
    request.command.executable = resolve_executable(jobserver::path_from_utf8(arguments[index++]));
    request.command.arguments.assign(arguments.begin() + static_cast<std::ptrdiff_t>(index),
                                     arguments.end());
    auto result{
        jobserver::Client::run(request, [](std::string const& stream, std::string const& text) {
            auto& output{stream == "stderr" ? std::cerr : std::cout};
            output << text;
            output.flush();
        })};
    return result ? *result : print_error(result.error());
}
}

auto wmain(int argc, wchar_t** argv) -> int {
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (auto index{0}; index != argc; ++index) {
        arguments.emplace_back(narrow(argv[index]));
    }
    if (arguments.size() < 2 || arguments[1] == "--help" || arguments[1] == "-h") {
        print_help();
        return arguments.size() < 2 ? 2 : 0;
    }
    auto const& command{arguments[1]};
    if (command == "run") {
        return run_command(std::vector<std::string>{arguments.begin() + 1, arguments.end()});
    }
    if (command == "status" || command == "history") {
        auto const json{arguments.size() >= 3 && arguments[2] == "--json"};
        auto result{jobserver::Client::status(command == "history")};
        if (!result) {
            return print_error(result.error());
        }
        if (json) {
            std::cout << *result << '\n';
        } else {
            print_status(*result);
        }
        return 0;
    }
    if (command == "show") {
        if (arguments.size() != 3) {
            std::cerr << "jobserver: show requires one job id\n";
            return 2;
        }
        auto result{jobserver::Client::status(true)};
        if (!result) {
            return print_error(result.error());
        }
        auto const status = Json::parse(*result, nullptr, false);
        for (auto const& job : status.value("jobs", Json::array())) {
            if (job.value("id", "") == arguments[2]) {
                std::cout << job.dump(2) << '\n';
                return 0;
            }
        }
        std::cerr << "jobserver: no job has id " << arguments[2] << '\n';
        return 3;
    }
    if (command == "cancel" || command == "kill") {
        if (arguments.size() != 3) {
            std::cerr << "jobserver: " << command << " requires one job id\n";
            return 2;
        }
        auto result{jobserver::Client::cancel(arguments[2], command == "kill")};
        return result ? 0 : print_error(result.error());
    }
    if (command == "logs") {
        if (arguments.size() != 3) {
            std::cerr << "jobserver: logs requires one job id\n";
            return 2;
        }
        auto const root{local_app_data() / "NukeTheBees" / "jobserver" / "data" / "logs"};
        for (auto const* stream : {"stdout", "stderr"}) {
            auto const path{root / (arguments[2] + "." + stream + ".log")};
            std::ifstream input{path, std::ios::binary};
            if (input) {
                std::cout << input.rdbuf();
            }
        }
        return 0;
    }
    if (command == "start") {
        auto result{jobserver::Client::start_daemon()};
        return result ? 0 : print_error(result.error());
    }
    if (command == "ping") {
        auto result{jobserver::Client::ping()};
        if (!result) {
            return print_error(result.error());
        }
        std::cout << "pong\n";
        return 0;
    }
    if (command == "shutdown") {
        auto result{jobserver::Client::shutdown()};
        return result ? 0 : print_error(result.error());
    }
    if (command == "recover") {
        if (arguments.size() != 3 || (arguments[2] != "--check" && arguments[2] != "--force")) {
            std::cerr << "jobserver: recover requires --check or --force\n";
            return 2;
        }
        if (arguments[2] == "--check") {
            auto assessment{jobserver::Client::check_daemon_recovery()};
            if (!assessment) {
                return print_error(assessment.error());
            }
            auto const yes_no = [](bool const value) { return value ? "yes" : "no"; };
            std::cout << "responsive:      " << yes_no(assessment->responsive) << '\n';
            std::cout << "authority valid: " << yes_no(assessment->authority_valid) << '\n';
            std::cout << "process running: " << yes_no(assessment->process_running) << '\n';
            std::cout << "recoverable:     " << yes_no(assessment->recoverable) << '\n';
            if (assessment->process_id != 0) {
                std::cout << "process id:      " << assessment->process_id << '\n';
                std::cout << "creation time:   " << assessment->creation_time << '\n';
                std::cout << "executable:      " << jobserver::path_to_utf8(assessment->executable)
                          << '\n';
            }
            std::cout << "reason:          " << assessment->reason << '\n';
            return 0;
        }
        auto recovered{jobserver::Client::force_recover_daemon()};
        if (!recovered) {
            return print_error(recovered.error());
        }
        std::cout << "Terminated the validated unresponsive daemon.\n";
        auto started{jobserver::Client::start_daemon()};
        return started ? 0 : print_error(started.error());
    }
    if (command == "doctor") {
        auto const root{local_app_data() / "NukeTheBees" / "jobserver"};
        std::cout << "install: " << (root / "bin") << '\n';
        std::cout << "data:    " << (root / "data") << '\n';
        auto failed{false};
        for (auto const& check : jobserver::cli::run_doctor()) {
            auto const* status = "PASS";
            if (check.status == jobserver::cli::DoctorStatus::warning) {
                status = "WARN";
            } else if (check.status == jobserver::cli::DoctorStatus::failure) {
                status = "FAIL";
                failed = true;
            }
            std::cout << status << "  " << check.name << ": " << check.detail << '\n';
        }
        return failed ? 4 : 0;
    }
    if (command == "version") {
        std::cout << "jobserver 0.1.0 (protocol " << jobserver::protocol::major_version << '.'
                  << jobserver::protocol::minor_version << ")\n";
        return 0;
    }
    std::cerr << "jobserver: unknown command " << command << '\n';
    return 2;
}
