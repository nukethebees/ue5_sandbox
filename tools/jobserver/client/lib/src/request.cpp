#include "jobserver/client/request.hpp"

#include "jobserver/protocol.hpp"

#include <utility>

namespace jobserver::client {
auto request(nlohmann::json message, std::filesystem::path const& endpoint) -> JsonResult {
    message["protocol"] = protocol::major_version;
    auto response{platform::exchange(endpoint, message.dump())};
    if (!response) {
        return JsonResult{std::unexpect, std::move(response).error()};
    }
    auto parsed = nlohmann::json::parse(*response, nullptr, false);
    if (!parsed.is_object()) {
        return JsonResult{std::unexpect, "invalid_response", "Invalid jobs-board response"};
    }
    if (parsed.value("type", "") == "error") {
        return JsonResult{std::unexpect,
                          parsed.value("code", "error"),
                          parsed.value("message", "Request failed")};
    }
    return parsed;
}
}
