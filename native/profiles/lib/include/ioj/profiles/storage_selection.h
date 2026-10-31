#pragma once

#include <ioj/profiles/storage_mode.h>

#include <filesystem>

namespace ioj::profiles {
struct StorageSelection {
    StorageMode mode{StorageMode::platform};
    std::filesystem::path root{};
    bool clone_shared{};
};
}
