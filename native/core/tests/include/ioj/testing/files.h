#pragma once

#include <ioj/files.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string_view>

namespace ioj::testing {
struct FileContents {
    std::filesystem::path path;
    std::string_view contents;
};

[[nodiscard]] inline auto write_files(std::initializer_list<FileContents> files)
    -> ::testing::AssertionResult {
    for (auto const& file : files) {
        std::ofstream output{file.path, std::ios::binary};
        if (!file.contents.empty()) {
            output.write(file.contents.data(), static_cast<std::streamsize>(file.contents.size()));
        }
        output.close();

        if (!output) {
            return ::testing::AssertionFailure()
                << "Failed to write fixture file: " << path_to_utf8(file.path);
        }
    }

    return ::testing::AssertionSuccess();
}
}
