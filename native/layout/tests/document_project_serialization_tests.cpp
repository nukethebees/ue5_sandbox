#include "document_test_support.hpp"
#include <ioj/layout/editor_session.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace ioj::layout::document_project_serialization_tests {

class DocumentProjectSerializationTest : public testing::TestWithParam<char const*> {
  protected:
    void SetUp() override {
        directory_ = std::filesystem::temp_directory_path() /
                     ("layout-project-roundtrip-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(directory_);
    }
    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
    }
    std::filesystem::path directory_;
};

TEST_P(DocumentProjectSerializationTest, CanonicalProjectSavesAndReloadsWithoutFurtherChanges) {
    EditorSession editor;
    auto document{
        editor.load(std::filesystem::path{IOJ_SOURCE_DIR} / "lispb/project.lispb", GetParam())};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(editor);
    std::vector<std::pair<std::string, std::string>> expected;
    for (auto const& source : document.sources()) {
        expected.emplace_back(source.path, source.text);
    }
    auto const destination{directory_ / "project.lispb"};
    ASSERT_TRUE(document.save_as(destination, GetParam())) << diagnostic_messages(editor);
    auto const reloaded{editor.load(destination, GetParam())};
    ASSERT_TRUE(reloaded.valid()) << diagnostic_messages(editor);
    auto const actual{reloaded.sources()};
    ASSERT_EQ(actual.size(), expected.size());
    auto const count{actual.size()};
    for (std::size_t index{}; index < count; ++index) {
        EXPECT_EQ(actual[index].path, expected[index].first);
        EXPECT_EQ(actual[index].text, expected[index].second) << actual[index].path;
    }
    EXPECT_EQ(reloaded.types().types().size(), document.types().types().size());
}

INSTANTIATE_TEST_SUITE_P(RepositoryTargets,
                         DocumentProjectSerializationTest,
                         testing::Values("sandbox-code", "compiler-ir", "cpp-compile-fixture"),
                         [](auto const& info) {
                             std::string name{info.param};
                             std::ranges::replace(name, '-', '_');
                             return name;
                         });

} // namespace ioj::layout::document_project_serialization_tests
