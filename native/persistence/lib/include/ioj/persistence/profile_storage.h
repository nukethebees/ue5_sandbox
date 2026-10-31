#pragma once

#include <ioj/persistence/profile_manager.h>

#include <expected>
#include <filesystem>
#include <optional>

namespace ioj::persistence {
enum class StorageMode : std::uint8_t { platform, shared, local, experimental };
struct StorageOptions {
    bool editor{};
    bool unattended{};
    bool local{};
    bool shared{};
    std::optional<std::string> experiment{};
    bool clone_shared{};
};
struct StorageSelection {
    StorageMode mode{StorageMode::platform};
    std::filesystem::path root{};
    bool clone_shared{};
};
auto resolve_profile_storage(StorageOptions options,
                             std::filesystem::path const& development_root,
                             std::filesystem::path const& local_root)
    -> std::expected<StorageSelection, std::string>;
auto make_file_profile_storage(std::filesystem::path root) -> ProfileStorage;
auto clone_profile_files(std::filesystem::path const& source_root,
                         std::filesystem::path const& destination_root,
                         std::string& error) -> bool;
struct MergeSummary {
    std::size_t source_profile_count{};
    std::size_t source_record_count{};
    std::size_t profile_count{};
    std::size_t unique_record_count{};
};
auto merge_profile_files(std::span<std::filesystem::path const> sources,
                         std::span<std::string const> excluded_profiles,
                         std::filesystem::path const& destination)
    -> std::expected<MergeSummary, std::string>;
}
