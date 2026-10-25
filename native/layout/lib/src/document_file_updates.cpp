#include "document_file_updates.hpp"

#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace ioj::layout {
void DocumentFileUpdates::write(std::filesystem::path path, std::string_view text) {
    paths_.push_back(std::move(path));
    texts_.emplace_back(text);
}
void DocumentFileUpdates::erase(std::filesystem::path path) {
    paths_.push_back(std::move(path));
    texts_.emplace_back(std::nullopt);
}
void DocumentFileUpdates::publish() {
    auto const count{paths_.size()};
    std::vector<std::filesystem::path> staged(count);
    std::vector<std::filesystem::path> backups(count);
    std::vector<bool> written(count);
    std::vector<bool> backed_up(count);
    std::vector<bool> published(count);
    try {
        for (std::size_t index{}; index < count; ++index) {
            auto const& path{paths_[index]};
            staged[index] = path.string() + ".layout-save-tmp";
            backups[index] = path.string() + ".layout-save-backup";
            if (std::filesystem::exists(staged[index]) || std::filesystem::exists(backups[index])) {
                throw std::runtime_error{"Save temporary already exists for: " + path.string()};
            }
            std::filesystem::create_directories(path.parent_path());
            if (texts_[index]) {
                std::ofstream output{staged[index], std::ios::binary | std::ios::trunc};
                written[index] = true;
                auto const& text{*texts_[index]};
                output.write(text.data(), static_cast<std::streamsize>(text.size()));
                output.close();
                if (!output) {
                    throw std::runtime_error{"Cannot stage source: " + path.string()};
                }
            }
        }
        for (std::size_t index{}; index < count; ++index) {
            auto const& path{paths_[index]};
            if (std::filesystem::exists(path)) {
                std::filesystem::rename(path, backups[index]);
                backed_up[index] = true;
            }
            if (texts_[index]) {
                std::filesystem::rename(staged[index], path);
                published[index] = true;
            }
        }
    } catch (std::runtime_error const& error) {
        std::string message{error.what()};
        for (auto index{count}; index != 0;) {
            --index;
            std::error_code rollback_error;
            if (published[index]) {
                std::filesystem::remove(paths_[index], rollback_error);
            }
            if (backed_up[index] && !rollback_error) {
                std::filesystem::rename(backups[index], paths_[index], rollback_error);
            }
            if (rollback_error) {
                std::format_to(std::back_inserter(message),
                               "; cannot restore {}: {}",
                               paths_[index].string(),
                               rollback_error.message());
            }
            if (written[index]) {
                std::error_code ignored;
                std::filesystem::remove(staged[index], ignored);
            }
        }
        throw std::runtime_error{message};
    }
    for (std::size_t index{}; index < count; ++index) {
        if (backed_up[index]) {
            std::error_code ignored;
            std::filesystem::remove(backups[index], ignored);
        }
    }
}
}
