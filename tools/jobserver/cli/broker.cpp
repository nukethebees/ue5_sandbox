#include "broker.hpp"

#include "jobserver/executor.hpp"
#include "jobserver/protocol.hpp"

#include <Windows.h>

#include <fcntl.h>
#include <io.h>

#include <array>
#include <iostream>
#include <mutex>
#include <thread>

namespace jobserver::cli {
auto broker() -> int {
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    std::mutex output_mutex;
    auto emit = [&](nlohmann::json const& message) {
        auto const frame{protocol::encode_frame(message.dump())};
        if (!frame) {
            return;
        }
        std::scoped_lock lock{output_mutex};
        std::cout.write(reinterpret_cast<char const*>(frame->data()),
                        static_cast<std::streamsize>(frame->size()));
        std::cout.flush();
    };
    auto session{Session::connect()};
    if (!session) {
        emit({{"type", "error"},
              {"code", session.error().code},
              {"message", session.error().message}});
        return 1;
    }
    LocalExecutor executor;
    std::thread{[&] {
        WaitForSingleObject((*session)->lost_event(), INFINITE);
        auto const error{(*session)->failure()};
        emit({{"type", "error"}, {"code", error.code}, {"message", error.message}});
        ExitProcess(1);
    }}.detach();
    std::mutex mutex;
    bool busy{}, executing{};
    std::stop_source pending;
    std::jthread worker;
    emit({{"type", "ready"}, {"client", (*session)->id().value}, {"protocol", 1}});
    while (std::cin && std::cout) {
        std::array<std::byte, 4> header{};
        if (!std::cin.read(reinterpret_cast<char*>(header.data()), 4)) {
            break;
        }
        auto size{protocol::decode_header(header)};
        if (!size) {
            emit({{"type", "error"}, {"message", size.error().message}});
            break;
        }
        std::string text(*size, '\0');
        if (!std::cin.read(text.data(), static_cast<std::streamsize>(text.size()))) {
            break;
        }
        try {
            auto message = nlohmann::json::parse(text);
            auto const type{message.at("type").get<std::string>()};
            std::unique_lock lock{mutex};
            if (type == "clear") {
                if (busy && !executing) {
                    pending.request_stop();
                } else {
                    emit({{"type", "error"},
                          {"code", "not_pending"},
                          {"message", "No pending command to clear"}});
                }
                continue;
            }
            if (type == "quit") {
                break;
            }
            if (type != "command") {
                throw std::runtime_error{"Expected command, clear or quit"};
            }
            if (busy) {
                emit({{"type", "error"},
                      {"code", "busy"},
                      {"message", "Broker already has a command"}});
                continue;
            }
            auto command_text{message.at("text").get<std::string>()};
            auto cwd{path_from_utf8(
                message.value("cwd", path_to_utf8(std::filesystem::current_path())))};
            auto metadata = message.value("metadata", nlohmann::json::object());
            metadata["command_text"] = command_text;
            metadata["cwd"] = path_to_utf8(cwd);
            if (!metadata.contains("worktree")) {
                metadata["worktree"] = path_to_utf8(cwd);
            }
            std::vector<GateClaim> gates;
            if (!message.at("gates").is_array()) {
                throw std::runtime_error{"gates must be an array"};
            }
            for (auto const& item : message.at("gates")) {
                auto const mode{item.at("mode").get<std::string>()};
                if (mode != "shared" && mode != "exclusive") {
                    throw std::runtime_error{"Invalid explicit lease mode"};
                }
                gates.push_back({item.at("name").get<std::string>(),
                                 mode == "exclusive" ? LeaseMode::exclusive : LeaseMode::shared});
            }
            if (worker.joinable()) {
                worker.join();
            }
            busy = true;
            executing = false;
            pending = std::stop_source{};
            worker = std::jthread{[&,
                                   command_text = std::move(command_text),
                                   cwd = std::move(cwd),
                                   metadata = std::move(metadata),
                                   gates = std::move(gates)] {
                std::optional<Grant> active;
                try {
                    auto grant{(*session)->acquire(gates, metadata, pending.get_token(), emit)};
                    if (!grant) {
                        if (grant.error().code != "cancelled") {
                            emit({{"type", "error"},
                                  {"code", grant.error().code},
                                  {"message", grant.error().message}});
                        }
                    } else {
                        active = *grant;
                        bool cleared{};
                        {
                            std::scoped_lock state_lock{mutex};
                            cleared = pending.stop_requested();
                            executing = !cleared;
                        }
                        if (cleared) {
                            static_cast<void>((*session)->release(*grant, 0));
                            active.reset();
                            emit({{"type", "cancelled"}, {"lease", grant->lease.value}});
                        } else {
                            auto const directory{cwd / ".local" / "jobserver-output"};
                            std::filesystem::create_directories(directory);
                            auto const prefix{directory / std::to_string(grant->command.value)};
                            OutputFiles files{prefix.wstring() + L".stdout.log",
                                              prefix.wstring() + L".stderr.log"};
                            emit({{"type", "starting"},
                                  {"client", grant->client.value},
                                  {"command", grant->command.value},
                                  {"lease", grant->lease.value},
                                  {"stdout", path_to_utf8(files.standard_output)},
                                  {"stderr", path_to_utf8(files.standard_error)}});
                            auto result{executor.run(powershell_command(command_text, cwd),
                                                     **session,
                                                     *grant,
                                                     gates,
                                                     &files)};
                            auto released{(*session)->release(*grant, result ? *result : 1)};
                            active.reset();
                            if (!result || !released) {
                                auto const error{!result ? result.error() : released.error()};
                                emit({{"type", "error"},
                                      {"code", error.code},
                                      {"message", error.message}});
                            } else {
                                emit({{"type", "completed"},
                                      {"command", grant->command.value},
                                      {"lease", grant->lease.value},
                                      {"exit_code", *result}});
                            }
                        }
                    }
                } catch (std::exception const& error) {
                    if (active) {
                        static_cast<void>((*session)->release(*active, 1));
                    }
                    emit({{"type", "error"}, {"message", error.what()}});
                }
                {
                    std::scoped_lock state_lock{mutex};
                    busy = false;
                    executing = false;
                }
                emit({{"type", "idle"}});
            }};
        } catch (std::exception const& error) {
            emit({{"type", "error"}, {"message", error.what()}});
        }
    }
    pending.request_stop();
    // On input EOF/quit, end the session, including any executing local tree.
    // ExitProcess closes the Job Object and pipe even if the worker is in a blocking wait.
    std::cout.flush();
    ExitProcess(0);
}
}
