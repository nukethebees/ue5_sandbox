#pragma once

#include <optional>
#include <string>

namespace ioj::profiles {
struct StorageOptions {
    bool editor{};
    bool unattended{};
    bool local{};
    bool shared{};
    std::optional<std::string> experiment{};
    bool clone_shared{};
};
}
