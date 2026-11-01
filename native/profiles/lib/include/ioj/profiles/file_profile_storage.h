#pragma once

#include <ioj/profiles/merge_summary.h>
#include <ioj/profiles/profile_storage.h>
#include <ioj/profiles/storage_options.h>
#include <ioj/profiles/storage_selection.h>

#include <expected>
#include <filesystem>
#include <optional>

namespace ioj::profiles {

auto resolve_profile_storage(StorageOptions options,
                             std::filesystem::path const& development_root,
                             std::filesystem::path const& local_root)
    -> std::expected<StorageSelection, std::string>;
auto make_file_profile_storage(std::filesystem::path const& root) -> ProfileStorage;
auto clone_profile_files(std::filesystem::path const& source_root,
                         std::filesystem::path const& destination_root,
                         std::string& error) -> bool;

auto merge_profile_files(std::span<std::filesystem::path const> sources,
                         std::span<std::string const> excluded_profiles,
                         std::filesystem::path const& destination)
    -> std::expected<MergeSummary, std::string>;
}
