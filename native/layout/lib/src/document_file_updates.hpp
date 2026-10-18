#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ioj::layout {
class DocumentFileUpdates {
  public:
    void write(std::filesystem::path path, std::string_view text);
    void erase(std::filesystem::path path);
    void publish();
  private:
    std::vector<std::filesystem::path> paths_;
    std::vector<std::optional<std::string>> texts_;
};
}
