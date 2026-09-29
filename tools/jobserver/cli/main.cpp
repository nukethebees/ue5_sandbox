#include "doctor.hpp"

#include "jobserver/client.hpp"
#include "jobserver/protocol.hpp"

#include <nlohmann/json.hpp>

#include <iostream>

namespace jobserver::cli {
auto error(Error const& value) -> int {
    std::cerr << "jobserver: " << value.code << ": " << value.message << '\n';
    return 1;
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
            std::cout << "jobserver status [--json] | trace | doctor | --version\n"
                         "jobserver start | ping | shutdown\n";
            return 0;
        }
        auto const& command{args[1]};
        if (args.size() > 2 && !(command == "status" && args.size() == 3 && args[2] == "--json")) {
            return cli::error({"usage", "Unexpected arguments"});
        }
        if (command == "status" || command == "trace") {
            auto result{command == "status" ? Client::status() : Client::trace()};
            if (!result) {
                return cli::error(result.error());
            }
            auto const json = nlohmann::json::parse(*result);
            std::cout << json.dump(args.size() > 2 && args[2] == "--json" ? -1 : 2) << '\n';
            return 0;
        }
        if (command == "version" || command == "--version") {
            std::cout << "jobserver 0.3.0 (protocol " << protocol::major_version << '.'
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
