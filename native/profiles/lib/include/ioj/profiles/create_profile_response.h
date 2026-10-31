#pragma once

#include <ioj/profiles/create_profile_result.h>

#include <string>

namespace ioj::profiles {
struct CreateProfileResponse {
    CreateProfileResult result{CreateProfileResult::persistence_failed};
    std::string profile_id{};
};
}
