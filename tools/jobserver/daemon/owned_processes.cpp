#include "owned_processes.hpp"

#include <Windows.h>

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>

namespace jobserver {
namespace {
using Json = nlohmann::json;

auto owned_process_command_json(Command const& command) -> Json {
    auto environment = Json::array();
    for (auto const& change : command.environment) {
        environment.push_back(
            {{"name", change.name}, {"value", change.value ? Json(*change.value) : Json(nullptr)}});
    }
    return {{"executable", path_to_utf8(command.executable)},
            {"arguments", command.arguments},
            {"working_directory", path_to_utf8(command.working_directory)},
            {"environment", std::move(environment)}};
}

auto group_json(OwnedProcessGroup const& group) -> Json {
    auto const started_ms{
        std::chrono::duration_cast<std::chrono::milliseconds>(group.started_at.time_since_epoch())
            .count()};
    return {{"job_id", group.job_id},
            {"name", group.metadata.name},
            {"kind", group.metadata.kind},
            {"task", group.metadata.task},
            {"worktree", path_to_utf8(group.metadata.worktree)},
            {"submit_directory", path_to_utf8(group.metadata.submit_directory)},
            {"command", owned_process_command_json(group.command)},
            {"root_pid", group.root.process_id},
            {"root_creation_time", group.root.creation_time},
            {"started_ms", started_ms}};
}
}

OwnedProcessStore::OwnedProcessStore(std::filesystem::path path)
    : path_{std::move(path)} {
    discard_recovered_records();
}

auto OwnedProcessStore::add(OwnedProcessGroup group) -> std::expected<void, Error> {
    std::scoped_lock const lock{mutex_};
    auto const job_id{group.job_id};
    auto const [found, inserted]{groups_.insert_or_assign(job_id, std::move(group))};
    static_cast<void>(found);
    static_cast<void>(inserted);
    auto persisted{persist_locked()};
    if (!persisted) {
        std::cerr << "Could not persist process ownership record: " << persisted.error().message
                  << '\n';
    }
    return {};
}

void OwnedProcessStore::remove(std::string const& job_id) noexcept {
    try {
        std::scoped_lock const lock{mutex_};
        if (groups_.erase(job_id) == 0) {
            return;
        }
        if (auto persisted{persist_locked()}; !persisted) {
            std::cerr << "Could not remove process ownership record: " << persisted.error().message
                      << '\n';
        }
    } catch (...) {
        std::cerr << "Could not remove process ownership record\n";
    }
}

auto OwnedProcessStore::snapshot() const -> std::vector<OwnedProcessGroup> {
    std::scoped_lock const lock{mutex_};
    std::vector<OwnedProcessGroup> result;
    result.reserve(groups_.size());
    for (auto const& [id, group] : groups_) {
        static_cast<void>(id);
        result.push_back(group);
    }
    return result;
}

auto OwnedProcessStore::recovered_record_count() const -> std::size_t {
    return recovered_record_count_;
}

auto OwnedProcessStore::persist_locked() -> std::expected<void, Error> {
    std::error_code filesystem_error;
    std::filesystem::create_directories(path_.parent_path(), filesystem_error);
    if (filesystem_error) {
        return std::unexpected(
            Error{"ownership_write_failed", "Could not create process ownership directory"});
    }

    auto groups = Json::array();
    for (auto const& [id, group] : groups_) {
        static_cast<void>(id);
        groups.push_back(group_json(group));
    }
    auto temporary{path_};
    temporary += ".tmp";
    {
        std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
        output << Json{{"groups", std::move(groups)}}.dump();
        if (!output) {
            return std::unexpected(
                Error{"ownership_write_failed", "Could not write process ownership data"});
        }
    }
    if (!MoveFileExW(
            temporary.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(temporary, filesystem_error);
        return std::unexpected(
            Error{"ownership_write_failed", "Could not publish process ownership data"});
    }
    return {};
}

void OwnedProcessStore::discard_recovered_records() noexcept {
    try {
        std::ifstream input{path_, std::ios::binary};
        auto const json = Json::parse(input, nullptr, false);
        if (json.is_object() && json.contains("groups") && json["groups"].is_array()) {
            recovered_record_count_ = json["groups"].size();
        }
        std::error_code error;
        std::filesystem::remove(path_, error);
    } catch (...) {
        recovered_record_count_ = 0;
    }
}
}
