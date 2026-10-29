#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/sexpr/indices.h>
#include <codegen/source_loader.h>
#include <lispb/compile_mode.h>
#include <lispb/compiled_target.h>
#include <lispb/compiler.h>
#include <lispb/compiler_storage.h>
#include <lispb/project.h>
#include <lispb/project_loader.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ranges>
#include <string>
#include <vector>

namespace lispb {
namespace {

class TemporaryProject {
  public:
    TemporaryProject() {
        static int sequence{};
        root_ = std::filesystem::temp_directory_path() /
                ("lispb-editable-project-test-" + std::to_string(++sequence));
        std::filesystem::create_directories(root_);
    }

    ~TemporaryProject() {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    void write(std::string const& name, std::string const& content) const {
        std::ofstream output{root_ / name, std::ios::binary | std::ios::trunc};
        output << content;
    }

    [[nodiscard]] auto path(std::string const& name) const -> std::filesystem::path {
        return root_ / name;
    }
  private:
    std::filesystem::path root_;
};

TEST(Compiler, RecompilesChangedSourcesAndRetainsProjectAndOverlays) {
    TemporaryProject files;
    files.write("types.lispb", "");
    files.write("project.lispb", R"((lispb-project :language-version 1 :project-root "."
      (cpp-schema test :types "types.lispb" :sources ("values.lispb")
        :output-root (project-path "generated"))))");
    files.write("values.lispb", R"((module values :header "Values.h" (record Before
    (projections (cpp)))))");

    Compiler compiler;
    auto& storage{compiler.storage()};
    auto const project{load_project(storage.syntax, files.path("project.lispb"))};
    auto const target{*project.find_target("test")};
    auto const compile_text{[&] {
        auto const& output{compiler.compile(project, target, files.path("build")).compilation};
        return output.text(output.artifacts.front());
    }};
    EXPECT_TRUE(compile_text().contains("struct Before"));

    files.write("values.lispb", R"((module values :header "Values.h" (record After
    (projections (cpp)))))");
    auto const changed{compile_text()};
    EXPECT_TRUE(changed.contains("struct After"));
    EXPECT_EQ(compile_text(), changed);
    EXPECT_TRUE(compiler.compile(project, target, files.path("build"), CompileMode::validate)
                    .compilation.artifacts.empty());

    static_cast<void>(storage.sources.overlay(files.path("values.lispb"),
                                              R"((module values :header "Values.h" (record Unsaved
    (projections (cpp)))))"));
    EXPECT_TRUE(compile_text().contains("struct Unsaved"));
    EXPECT_EQ(project.target_name(target), "test");
    EXPECT_FALSE(std::filesystem::exists(files.path("generated/Values.h")));
}

}
}
