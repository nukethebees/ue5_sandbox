#pragma once

#include <lispb/schema/type_identity.h>

#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {
struct GraphScope {
    auto operator==(GraphScope const&) const -> bool = default;

    // Empty means project overview; membership allows later scopes spanning modules.
    std::optional<std::string> module;
    std::vector<lispb::schema::TypeIdentity> members;
};
}
