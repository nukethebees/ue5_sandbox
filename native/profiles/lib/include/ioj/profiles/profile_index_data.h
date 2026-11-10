#pragma once

#include <ioj/profiles/profile_metadata.h>
#include <ioj/profiles/save_version.h>

#include <vector>

namespace ioj::profiles {
struct ProfileIndexData {
    auto operator==(ProfileIndexData const&) const -> bool = default;

    std::int32_t save_version{current_save_version};
    std::string active_profile_id{};
    std::vector<ProfileMetadata> profiles{};
};
}
