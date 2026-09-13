#pragma once

#include <sandbox/level_authoring/CampaignDefinition.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ml::level_authoring {
struct CampaignDefinitionDecodeError {
    std::string path{};
    std::string message{};
};

struct CampaignDefinitionReadResult {
    std::optional<CampaignDefinition> definition{};
    std::string script_error{};
    std::vector<CampaignDefinitionDecodeError> decode_errors{};

    explicit operator bool() const noexcept { return definition.has_value(); }
};

class CampaignDefinitionReader final {
  public:
    CampaignDefinitionReader() = default;
    explicit CampaignDefinitionReader(std::string script_library_root);

    [[nodiscard]] auto read_source(std::string_view source) const -> CampaignDefinitionReadResult;
  private:
    std::string script_library_root_{};
};
} // namespace ml::level_authoring
