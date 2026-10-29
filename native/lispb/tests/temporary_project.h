#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace lispb::test_support {

class TemporaryProject {
  public:
    TemporaryProject(std::string_view const suite, std::string_view const name)
        : root_{std::filesystem::temp_directory_path() /
                ("sandbox-" + std::string{suite} + "-codegen-" + std::string{name})} {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
        std::filesystem::create_directories(root_);
        write(".clang-format", "BasedOnStyle: LLVM\nLineEnding: LF\n");
    }

    ~TemporaryProject() {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    void write(std::filesystem::path const& relative_path, std::string_view const content) const {
        auto const path{root_ / relative_path};
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output{path, std::ios::binary};
        if (!output) {
            throw std::runtime_error{"Cannot write test file: " + path.string()};
        }
        output << content;
    }

    auto read(std::filesystem::path const& relative_path) const -> std::string {
        auto const path{root_ / relative_path};
        std::ifstream input{path, std::ios::binary};
        if (!input) {
            throw std::runtime_error{"Cannot read test file: " + path.string()};
        }
        return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    }

    auto path(std::filesystem::path const& relative_path) const -> std::filesystem::path {
        return root_ / relative_path;
    }

  private:
    std::filesystem::path root_;
};

}
