#include "log_store.hpp"

#include <algorithm>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace jobserver {
namespace log_detail {
inline constexpr std::string_view truncation_marker{"\n[jobserver: log size limit reached]\n"};

auto valid_id(std::string_view const id) -> bool {
    return id.size() == 32 && std::ranges::all_of(id, [](char const character) {
               return (character >= '0' && character <= '9') ||
                      (character >= 'a' && character <= 'f');
           });
}
}

CappedLog::CappedLog(std::filesystem::path const& path, std::size_t const maximum_bytes)
    : output_{path, std::ios::binary | std::ios::trunc}
    , maximum_bytes_{maximum_bytes} {}

void CappedLog::write(std::string const& text) {
    if (!output_ || truncated_) {
        return;
    }
    auto const marker_bytes{std::min(maximum_bytes_, log_detail::truncation_marker.size())};
    auto const count{std::min(text.size(), maximum_bytes_ - written_)};
    output_.write(text.data(), static_cast<std::streamsize>(count));
    written_ += count;
    if (count < text.size()) {
        output_.seekp(static_cast<std::streamoff>(maximum_bytes_ - marker_bytes));
        output_.write(log_detail::truncation_marker.data(),
                      static_cast<std::streamsize>(marker_bytes));
        truncated_ = true;
    }
    output_.flush();
}

void CappedLog::close() {
    output_.close();
}
auto CappedLog::available() const -> bool {
    return output_.is_open();
}

JobLogs::JobLogs(LogStore& store,
                 std::string id,
                 std::filesystem::path const& directory,
                 std::size_t const maximum_bytes)
    : store_{store}
    , id_{std::move(id)}
    , stdout_{directory / (id_ + ".stdout.log"), maximum_bytes}
    , stderr_{directory / (id_ + ".stderr.log"), maximum_bytes} {}

JobLogs::~JobLogs() {
    stdout_.close();
    stderr_.close();
    store_.finish(id_);
}
void JobLogs::write(std::string const& stream, std::string const& text) {
    (stream == "stderr" ? stderr_ : stdout_).write(text);
}
auto JobLogs::available() const -> bool {
    return stdout_.available() && stderr_.available();
}

LogStore::LogStore(std::filesystem::path directory,
                   std::size_t const maximum_bytes,
                   std::size_t const retention)
    : directory_{std::move(directory)}
    , maximum_bytes_{maximum_bytes}
    , retention_{retention} {
    std::error_code error;
    std::filesystem::create_directories(directory_, error);
    try {
        prune();
    } catch (...) {
        std::cerr << "Could not prune old job logs\n";
    }
}

auto LogStore::open(std::string const& id) -> std::unique_ptr<JobLogs> {
    if (!log_detail::valid_id(id)) {
        return {};
    }
    std::scoped_lock const lock{mutex_};
    std::error_code error;
    auto const status{std::filesystem::symlink_status(directory_, error)};
    if (error || !std::filesystem::is_directory(status)) {
        return {};
    }
    active_.insert(id);
    try {
        return std::unique_ptr<JobLogs>{new JobLogs{*this, id, directory_, maximum_bytes_}};
    } catch (...) {
        active_.erase(id);
        throw;
    }
}

void LogStore::finish(std::string const& id) noexcept {
    try {
        std::scoped_lock const lock{mutex_};
        active_.erase(id);
        std::error_code error;
        for (auto const suffix : {".stdout.log", ".stderr.log"}) {
            std::filesystem::last_write_time(
                directory_ / (id + suffix), std::filesystem::file_time_type::clock::now(), error);
            error.clear();
        }
        prune();
    } catch (...) {
        std::cerr << "Could not prune completed job logs\n";
    }
}

void LogStore::prune() {
    struct Files {
        std::string id;
        std::vector<std::filesystem::path> paths;
        std::filesystem::file_time_type modified{std::filesystem::file_time_type::min()};
    };
    std::error_code error;
    auto const directory_status{std::filesystem::symlink_status(directory_, error)};
    if (error || !std::filesystem::is_directory(directory_status)) {
        return;
    }
    std::unordered_map<std::string, Files> jobs;
    auto iterator{std::filesystem::directory_iterator{directory_, error}};
    auto const end{std::filesystem::directory_iterator{}};
    for (; !error && iterator != end; iterator.increment(error)) {
        auto const& file{*iterator};
        auto const status{file.symlink_status(error)};
        if (error) {
            return;
        }
        auto const name{file.path().filename().string()};
        if (!std::filesystem::is_regular_file(status) || name.size() < 32 ||
            !log_detail::valid_id(std::string_view{name}.substr(0, 32)) ||
            (name.substr(32) != ".stdout.log" && name.substr(32) != ".stderr.log")) {
            continue;
        }
        auto const id{name.substr(0, 32)};
        if (active_.contains(id)) {
            continue;
        }
        auto const modified{file.last_write_time(error)};
        if (error) {
            return;
        }
        auto& job{jobs[id]};
        job.id = id;
        job.paths.push_back(file.path());
        job.modified = std::max(job.modified, modified);
    }
    if (error) {
        return;
    }
    std::vector<Files> ordered;
    for (auto& [id, job] : jobs) {
        static_cast<void>(id);
        ordered.push_back(std::move(job));
    }
    std::ranges::sort(ordered, [](Files const& left, Files const& right) {
        return left.modified != right.modified ? left.modified > right.modified
                                               : left.id > right.id;
    });
    auto const retained{std::min(retention_, ordered.size())};
    for (std::size_t index{}; index < retained; ++index) {
        for (auto const& path : ordered[index].paths) {
            auto const size{std::filesystem::file_size(path, error)};
            if (!error && size > maximum_bytes_) {
                auto const marker_bytes{
                    std::min(maximum_bytes_, log_detail::truncation_marker.size())};
                std::filesystem::resize_file(path, maximum_bytes_ - marker_bytes, error);
                if (!error) {
                    std::ofstream output{path, std::ios::binary | std::ios::app};
                    output.write(log_detail::truncation_marker.data(),
                                 static_cast<std::streamsize>(marker_bytes));
                }
            }
            error.clear();
        }
    }
    for (auto index{retention_}; index < ordered.size(); ++index) {
        for (auto const& path : ordered[index].paths) {
            std::filesystem::remove(path, error);
            if (error) {
                std::cerr << "Could not remove expired job log\n";
                error.clear();
            }
        }
    }
}
}
