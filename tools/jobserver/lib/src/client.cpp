#include "jobserver/client.hpp"

#include "jobserver/protocol.hpp"

#include <Windows.h>

namespace jobserver {
auto request(nlohmann::json message, std::wstring const& endpoint)
    -> std::expected<nlohmann::json, Error> {
    HANDLE pipe{};
    for (;;) {
        pipe = CreateFileW(
            endpoint.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (pipe != INVALID_HANDLE_VALUE) {
            break;
        }
        if (GetLastError() != ERROR_PIPE_BUSY || !WaitNamedPipeW(endpoint.c_str(), 5000)) {
            return std::unexpected(
                Error{"unavailable",
                      "Jobs board unavailable; ask the maintainer to install/start the jobserver"});
        }
    }

    message["protocol"] = protocol::major_version;
    auto const sent{transport::write_message(pipe, message.dump())};
    if (!sent) {
        CloseHandle(pipe);
        return std::unexpected(sent.error());
    }
    auto const response{transport::read_message(pipe)};
    CloseHandle(pipe);
    if (!response) {
        return std::unexpected(response.error());
    }
    auto parsed = nlohmann::json::parse(*response, nullptr, false);
    if (!parsed.is_object()) {
        return std::unexpected(Error{"invalid_response", "Invalid jobs-board response"});
    }
    if (parsed.value("type", "") == "error") {
        return std::unexpected(
            Error{parsed.value("code", "error"), parsed.value("message", "Request failed")});
    }
    return parsed;
}
}
