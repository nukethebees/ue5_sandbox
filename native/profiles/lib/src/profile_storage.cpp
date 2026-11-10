#include <ioj/files.h>
#include <ioj/profiles/file_profile_storage.h>
#include <ioj/profiles/profile_id.h>
#include <ioj/profiles/profile_metadata.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace ioj::profiles {
namespace profile_files {
using Json = nlohmann::json;
inline constexpr std::string_view index_name{"profiles.json"};
inline constexpr std::uintmax_t maximum_file_size{64 * 1024 * 1024};

auto with_suffix(std::filesystem::path const& path, std::string_view suffix)
    -> std::filesystem::path {
    auto result{path};
    result += suffix;
    return result;
}

auto valid_identifier(std::string const& value) -> bool {
    return !value.empty() && value.size() <= 64 && std::ranges::all_of(value, [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '_' || c == '-';
    });
}
auto checksum(std::string_view bytes) -> std::uint32_t {
    std::uint32_t crc{0xffffffffu};
    for (unsigned char const byte : bytes) {
        crc ^= byte;
        for (int bit{}; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1u) != 0 ? 0xedb88320u : 0u);
        }
    }
    return ~crc;
}
auto encode(ScoreRecord const& value) -> Json {
    return {{"date", value.date},
            {"level", value.level_name},
            {"team", to_serialized_string(value.team)},
            {"outcome", to_serialized_string(value.outcome)},
            {"end_reason", to_serialized_string(value.end_reason)},
            {"kills", value.kills},
            {"seconds", value.time_seconds}};
}
auto encode(ProfileResultsData const& value) -> Json {
    auto rows = Json::array();
    for (auto const& record : value.score_records) {
        rows.push_back(encode(record));
    }
    return {{"version", value.save_version}, {"records", std::move(rows)}};
}
auto encode(ProfileIndexData const& value) -> Json {
    auto rows = Json::array();
    for (auto const& profile : value.profiles) {
        rows.push_back({{"id", profile.profile_id},
                        {"name", profile.display_name},
                        {"created", profile.created_at},
                        {"last_played", profile.last_played_at},
                        {"duration", profile.total_simulation_duration_seconds},
                        {"kills", profile.total_kills},
                        {"outcomes", profile.outcome_count},
                        {"unlock_all", profile.debug_settings.unlock_all_missions},
                        {"start_paused", profile.debug_settings.start_levels_paused}});
    }
    return {{"version", value.save_version},
            {"active", value.active_profile_id},
            {"profiles", std::move(rows)}};
}
auto decode(Json const& json, ProfileResultsData& value) -> bool {
    ProfileResultsData result;
    result.save_version = json.at("version").get<std::int32_t>();
    if (result.save_version != current_save_version) {
        return false;
    }

    auto const& rows{json.at("records")};
    if (!rows.is_array()) {
        return false;
    }
    for (auto const& row : rows) {
        auto const team{try_parse_serialized_team(row.at("team").get<std::string>())};
        auto const outcome{
            try_parse_serialized_level_outcome(row.at("outcome").get<std::string>())};
        auto const reason{
            sim::try_parse_serialized_run_end_reason(row.at("end_reason").get<std::string>())};
        ScoreRecord record{.date = row.at("date").get<Timestamp>(),
                           .level_name = row.at("level").get<std::string>(),
                           .kills = row.at("kills").get<std::int32_t>(),
                           .time_seconds = row.at("seconds").get<float>()};
        if (!team || !outcome || !reason || record.level_name.empty() || record.kills < 0 ||
            !std::isfinite(record.time_seconds) || record.time_seconds < 0.f) {
            return false;
        }
        record.team = *team;
        record.outcome = *outcome;
        record.end_reason = *reason;
        result.score_records.push_back(std::move(record));
    }
    value = std::move(result);
    return true;
}
auto decode(Json const& json, ProfileIndexData& value) -> bool {
    ProfileIndexData result;
    result.save_version = json.at("version").get<std::int32_t>();
    if (result.save_version != current_save_version) {
        return false;
    }

    result.active_profile_id = json.at("active").get<std::string>();
    auto const& rows{json.at("profiles")};
    if (!rows.is_array()) {
        return false;
    }
    for (auto const& row : rows) {
        ProfileMetadata profile{
            .profile_id = row.at("id").get<std::string>(),
            .display_name = row.at("name").get<std::string>(),
            .created_at = row.at("created").get<Timestamp>(),
            .last_played_at = row.at("last_played").get<Timestamp>(),
            .total_simulation_duration_seconds = row.at("duration").get<float>(),
            .total_kills = row.at("kills").get<std::int32_t>(),
            .outcome_count = row.at("outcomes").get<std::int32_t>(),
            .debug_settings = {.unlock_all_missions = row.at("unlock_all").get<bool>(),
                               .start_levels_paused = row.at("start_paused").get<bool>()}};
        if (!valid_identifier(profile.profile_id) || profile.display_name.empty() ||
            !std::isfinite(profile.total_simulation_duration_seconds) ||
            profile.total_simulation_duration_seconds < 0.f || profile.total_kills < 0 ||
            profile.outcome_count < 0 ||
            std::ranges::any_of(
                result.profiles,
                [&](auto const& existing) { return existing.profile_id == profile.profile_id; })) {
            return false;
        }
        result.profiles.push_back(std::move(profile));
    }
    value = std::move(result);
    return true;
}

auto read_envelope(std::filesystem::path const& path, Json& payload, std::uint64_t& generation)
    -> bool {
    std::error_code error;
    auto const size{std::filesystem::file_size(path, error)};
    if (error || size > maximum_file_size) {
        return false;
    }
    std::ifstream input{path, std::ios::binary};
    auto envelope = Json::parse(input, nullptr, false);
    if (envelope.is_discarded() || !envelope.is_object() || envelope.value("format", 0) != 1) {
        return false;
    }
    auto const bytes{envelope.at("payload").get<std::string>()};
    if (checksum(bytes) != envelope.at("checksum").get<std::uint32_t>()) {
        return false;
    }
    payload = Json::parse(bytes, nullptr, false);
    generation = envelope.at("generation").get<std::uint64_t>();
    return !payload.is_discarded();
}

auto write_and_flush(std::filesystem::path const& path, std::string_view const bytes) -> bool {
#ifdef _WIN32
    auto const file{CreateFileW(
        path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written{};
    auto const succeeded{
        bytes.size() <= std::numeric_limits<DWORD>::max() &&
        WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
        written == bytes.size() && FlushFileBuffers(file)};
    CloseHandle(file);
    return succeeded;
#else
    auto const file{::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600)};
    if (file < 0) {
        return false;
    }
    std::size_t offset{};
    while (offset < bytes.size()) {
        auto const written{::write(file, bytes.data() + offset, bytes.size() - offset)};
        if (written <= 0) {
            ::close(file);
            return false;
        }
        offset += static_cast<std::size_t>(written);
    }
    auto const succeeded{::fsync(file) == 0};
    ::close(file);
    return succeeded;
#endif
}
auto replace_file(std::filesystem::path const& path, std::filesystem::path const& temporary)
    -> bool {
#ifdef _WIN32
    return MoveFileExW(temporary.c_str(),
                       path.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        return false;
    }
    auto const directory{::open(path.parent_path().c_str(), O_RDONLY | O_DIRECTORY)};
    if (directory < 0) {
        return false;
    }
    auto const flushed{::fsync(directory) == 0};
    ::close(directory);
    return flushed;
#endif
}

template <typename Data>
auto load_data(std::filesystem::path const& path, Data& value) -> LoadResult {
    bool found{};
    for (auto const& candidate : {path, with_suffix(path, ".bak")}) {
        std::error_code error;
        auto const exists{std::filesystem::exists(candidate, error)};
        if (error) {
            found = true;
            continue;
        }
        if (!exists) {
            continue;
        }
        found = true;
        try {
            Json payload;
            std::uint64_t generation{};
            Data result;
            if (read_envelope(candidate, payload, generation) && decode(payload, result)) {
                value = std::move(result);
                return LoadResult::succeeded;
            }
        } catch (Json::exception const&) {
            // Try the backup when the primary file contains malformed data.
            continue;
        }
    }
    return found ? LoadResult::failed : LoadResult::not_found;
}
template <typename Data>
auto save_data(std::filesystem::path const& path, Data const& value) -> bool {
    auto const temporary{with_suffix(path, ".tmp." + make_profile_id())};
    try {
        std::filesystem::create_directories(path.parent_path());
        Json old;
        std::uint64_t generation{};
        auto valid_primary{false};
        if (std::filesystem::exists(path)) {
            try {
                Data previous;
                valid_primary = read_envelope(path, old, generation) && decode(old, previous);
            } catch (Json::exception const&) {
                // Preserve the backup when the primary file contains malformed data.
                valid_primary = false;
            }
        }
        auto const encoded = encode(value);
        Data validated;
        if (!decode(encoded, validated)) {
            return false;
        }
        auto const payload{encoded.dump()};
        auto const bytes{Json{
            {"format", 1},
            {"generation", generation + 1},
            {"checksum", checksum(payload)},
            {"payload",
             payload}}.dump()};
        if (!write_and_flush(temporary, bytes)) {
            std::filesystem::remove(temporary);
            return false;
        }
        if (valid_primary) {
            std::filesystem::copy_file(
                path, with_suffix(path, ".bak"), std::filesystem::copy_options::overwrite_existing);
        }
        if (replace_file(path, temporary)) {
            return true;
        }
        // NOLINTNEXTLINE(bugprone-empty-catch) -- share cleanup and failure reporting below.
    } catch (std::exception const&) {}
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    return false;
}

class FileLock {
  public:
    explicit FileLock(std::filesystem::path const& root) {
        std::filesystem::create_directories(root.parent_path());
        auto const path{with_suffix(root, ".lock")};
#ifdef _WIN32
        handle_ = CreateFileW(path.c_str(),
                              GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr,
                              OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) {
            return;
        }
#else
        handle_ = ::open(path.c_str(), O_RDWR | O_CREAT, 0600);
        if (handle_ < 0) {
            return;
        }
#endif
        auto const deadline{std::chrono::steady_clock::now() + std::chrono::seconds{5}};
        do {
            bool acquired{};
#ifdef _WIN32
            OVERLAPPED overlap{};
            acquired = LockFileEx(handle_,
                                  LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                                  0,
                                  1,
                                  0,
                                  &overlap) != 0;
#else
            acquired = ::flock(handle_, LOCK_EX | LOCK_NB) == 0;
#endif
            if (acquired) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        } while (std::chrono::steady_clock::now() < deadline);
#ifdef _WIN32
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
#else
        ::close(handle_);
        handle_ = -1;
#endif
    }
    ~FileLock() {
#ifdef _WIN32
        if (handle_ != INVALID_HANDLE_VALUE) {
            OVERLAPPED overlap{};
            UnlockFileEx(handle_, 0, 1, 0, &overlap);
            CloseHandle(handle_);
        }
#else
        if (handle_ >= 0) {
            ::flock(handle_, LOCK_UN);
            ::close(handle_);
        }
#endif
    }
    FileLock(FileLock const&) = delete;
    auto operator=(FileLock const&) -> FileLock& = delete;
    auto locked() const -> bool {
#ifdef _WIN32
        return handle_ != INVALID_HANDLE_VALUE;
#else
        return handle_ >= 0;
#endif
    }
  private:
#ifdef _WIN32
    HANDLE handle_{INVALID_HANDLE_VALUE};
#else
    int handle_{-1};
#endif
};

struct StorageState {
    std::filesystem::path root{};
    std::mutex process_lock{};
};
}

auto resolve_profile_storage(StorageOptions options,
                             std::filesystem::path const& development_root,
                             std::filesystem::path const& local_root)
    -> std::expected<StorageSelection, std::string> {
    if (!options.editor) {
        return StorageSelection{.root = local_root};
    }
    if (static_cast<int>(options.local) + static_cast<int>(options.shared) +
            static_cast<int>(options.experiment.has_value()) >
        1) {
        return std::unexpected{"Conflicting SpaceSave storage selectors."};
    }
    StorageSelection selection{.mode =
                                   options.unattended ? StorageMode::local : StorageMode::shared,
                               .clone_shared = options.clone_shared};
    if (options.local) {
        selection.mode = StorageMode::local;
    } else if (options.shared) {
        selection.mode = StorageMode::shared;
    } else if (options.experiment) {
        std::ranges::transform(*options.experiment,
                               options.experiment->begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (options.experiment->size() > 48 ||
            !profile_files::valid_identifier(*options.experiment)) {
            return std::unexpected{
                "SpaceSaveExperiment must contain only letters, digits, '_' or '-'."};
        }
        selection.mode = StorageMode::experimental;
    }
    if (options.clone_shared && selection.mode != StorageMode::experimental) {
        return std::unexpected{"SpaceSaveCloneShared requires SpaceSaveExperiment."};
    }
    selection.root = selection.mode == StorageMode::shared ? development_root / "Shared"
                   : selection.mode == StorageMode::experimental
                       ? development_root / "Experiments" / *options.experiment
                       : local_root;
    return selection;
}
auto make_file_profile_storage(std::filesystem::path const& root) -> ProfileStorage {
    using namespace profile_files;
    auto state{std::make_shared<StorageState>()};
    state->root = std::filesystem::absolute(root).lexically_normal();
    return {
        .with_exclusive_access =
            [state](std::function<bool()> const& operation) {
                std::scoped_lock process_lock{state->process_lock};
                try {
                    FileLock lock{state->root};
                    return lock.locked() && operation();
                } catch (std::filesystem::filesystem_error const&) {
                    return false;
                }
            },
        .load_index =
            [state](ProfileIndexData& value) { return load_data(state->root / index_name, value); },
        .save_index =
            [state](ProfileIndexData const& value) {
                return save_data(state->root / index_name, value);
            },
        .load_results =
            [state](std::string const& id, ProfileResultsData& value) {
                return valid_identifier(id)
                         ? load_data(state->root / ("profile_" + id + ".json"), value)
                         : LoadResult::failed;
            },
        .save_results =
            [state](std::string const& id, ProfileResultsData const& value) {
                return valid_identifier(id) &&
                       save_data(state->root / ("profile_" + id + ".json"), value);
            },
    };
}
auto clone_profile_files(std::filesystem::path const& source_root,
                         std::filesystem::path const& destination_root,
                         std::string& error) -> bool {
    auto const staging{
        profile_files::with_suffix(destination_root, ".creating." + make_profile_id())};
    auto source{make_file_profile_storage(source_root)};
    auto destination{make_file_profile_storage(destination_root)};
    auto const copied{destination.with_exclusive_access([&] {
        if (std::filesystem::exists(destination_root / profile_files::index_name) ||
            std::filesystem::exists(destination_root / "profiles.json.bak")) {
            return true;
        }
        return source.with_exclusive_access([&] {
            ProfileIndexData index;
            if (source.load_index(index) != LoadResult::succeeded ||
                index.save_version != current_save_version) {
                return false;
            }
            for (auto const& profile : index.profiles) {
                ProfileResultsData results;
                if (source.load_results(profile.profile_id, results) != LoadResult::succeeded ||
                    results.save_version != current_save_version) {
                    return false;
                }
            }
            if (std::filesystem::exists(destination_root) &&
                !std::filesystem::remove(destination_root)) {
                return false;
            }
            std::filesystem::create_directories(staging);
            for (auto const& entry : std::filesystem::directory_iterator{source_root}) {
                if (entry.is_regular_file() &&
                    (entry.path().extension() == ".json" || entry.path().extension() == ".bak")) {
                    std::filesystem::copy_file(entry.path(), staging / entry.path().filename());
                }
            }
            std::filesystem::rename(staging, destination_root);
            return true;
        });
    })};
    if (!copied) {
        error = "Cannot clone profiles into an uninitialized or invalid destination";
        std::error_code ignored;
        std::filesystem::remove_all(staging, ignored);
    }
    return copied;
}
auto merge_profile_files(std::span<std::filesystem::path const> sources,
                         std::span<std::string const> excluded_profiles,
                         std::filesystem::path const& destination)
    -> std::expected<MergeSummary, std::string> {
    if (sources.empty()) {
        return std::unexpected{"No source roots were supplied."};
    }
    struct MergedProfile {
        ProfileMetadata metadata;
        ProfileResultsData results;
    };
    std::map<std::string, MergedProfile> profiles;
    MergeSummary summary;
    for (auto const& source : sources) {
        auto storage{make_file_profile_storage(source)};
        if (!storage.with_exclusive_access([&] {
                ProfileIndexData index;
                if (storage.load_index(index) != LoadResult::succeeded ||
                    index.save_version != current_save_version) {
                    return false;
                }
                for (auto const& metadata : index.profiles) {
                    if (std::ranges::contains(excluded_profiles, metadata.profile_id)) {
                        continue;
                    }
                    ProfileResultsData results;
                    if (storage.load_results(metadata.profile_id, results) !=
                            LoadResult::succeeded ||
                        results.save_version != current_save_version) {
                        return false;
                    }
                    ++summary.source_profile_count;
                    summary.source_record_count += results.score_records.size();
                    auto [found, inserted]{
                        profiles.try_emplace(metadata.profile_id, MergedProfile{metadata, {}})};
                    auto& merged{found->second};
                    if (!inserted) {
                        if (merged.metadata.created_at == 0 ||
                            (metadata.created_at != 0 &&
                             metadata.created_at < merged.metadata.created_at)) {
                            merged.metadata.created_at = metadata.created_at;
                        }
                        if (metadata.last_played_at > merged.metadata.last_played_at) {
                            merged.metadata.display_name = metadata.display_name;
                            merged.metadata.last_played_at = metadata.last_played_at;
                        }
                        merged.metadata.debug_settings.unlock_all_missions |=
                            metadata.debug_settings.unlock_all_missions;
                        merged.metadata.debug_settings.start_levels_paused |=
                            metadata.debug_settings.start_levels_paused;
                    }
                    for (auto const& record : results.score_records) {
                        if (!std::ranges::contains(merged.results.score_records, record)) {
                            merged.results.score_records.push_back(record);
                        }
                    }
                }
                return true;
            })) {
            return std::unexpected{"Unsupported or incomplete source profile set: " +
                                   path_to_utf8(source)};
        }
    }
    if (profiles.empty()) {
        return std::unexpected{"No profiles remained after exclusions."};
    }
    auto const staging{profile_files::with_suffix(destination, ".merging." + make_profile_id())};
    auto destination_storage{make_file_profile_storage(destination)};
    auto const published{destination_storage.with_exclusive_access([&] {
        if (std::filesystem::exists(destination)) {
            return false;
        }
        auto storage{make_file_profile_storage(staging)};
        ProfileIndexData index;
        for (auto& [id, profile] : profiles) {
            update_profile_metadata(profile.metadata, profile.results.score_records);
            if (!storage.save_results(id, profile.results)) {
                return false;
            }
            index.profiles.push_back(profile.metadata);
            summary.unique_record_count += profile.results.score_records.size();
        }
        auto const default_profile{
            std::ranges::find(index.profiles, "default_profile", &ProfileMetadata::profile_id)};
        if (default_profile != index.profiles.end()) {
            std::iter_swap(index.profiles.begin(), default_profile);
        }
        index.active_profile_id = index.profiles.front().profile_id;
        if (!storage.save_index(index)) {
            return false;
        }
        std::filesystem::rename(staging, destination);
        return true;
    })};
    if (!published) {
        std::error_code ignored;
        std::filesystem::remove_all(staging, ignored);
        return std::unexpected{"Destination exists or merged profiles could not be published."};
    }
    summary.profile_count = profiles.size();
    return summary;
}
}
