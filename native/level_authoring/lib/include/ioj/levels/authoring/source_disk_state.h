#pragma once

#include <string>

namespace ioj::levels::authoring {
struct SourceDiskState {
    auto operator==(SourceDiskState const&) const -> bool = default;

    bool exists{};
    std::string source{};
};
}
