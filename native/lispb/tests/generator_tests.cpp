#include "compiler_test_context.h"
#include "test_utilities.h"

#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/record_kind.h>
#include <codegen/ast/type_operation.h>
#include <codegen/generated_file.h>
#include <codegen/generator.h>
#include <codegen/schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/facade_schemas.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/source_loader.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/project.h>
#include <lispb/project_loader.h>
#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace lispb::codegen {
namespace {

auto example_manifest(CompilerTestContext& context) -> Manifest {
    auto const handle{context.storage.cpp_types.add(
        "FHandle", {context.storage.cpp_types.dependency("FHandle", "Project/Handle.h")})};
    context.storage.cpp_types.set_operation(
        handle, TypeOperation::remove_at_swap, "remove_at_swap");
    return parse_test_manifest(context,
                               R"(
        (module example :header "Generated.h" :source "Generated.cpp"
          :header-include "Project/Generated.h" :namespace example :include-order ("Project/" "SandboxCore/")
          (struct FData :export-specifier EXAMPLE_API
            (member handles array @handle)
            (member weights array float)
            (member more_handles array @handle))))",
                               {{"handle", handle}});
}

TEST(Generator, LowersDynamicSoaIntoNativeOwnerAndViews) {
    CompilerTestContext context;
    auto const files{render_test_manifest(context, example_manifest(context))};
    ASSERT_EQ(files.size(), 2);
    auto const& header{files.front().content};
    auto const& source{files.back().content};
    EXPECT_NE(header.find("namespace f_data"), std::string::npos);
    EXPECT_NE(header.find("struct EXAMPLE_API ConstView"), std::string::npos);
    EXPECT_NE(header.find("ResolvedViewBase<"), std::string::npos);
    EXPECT_NE(header.find("std::pmr::memory_resource&"), std::string::npos);
    EXPECT_NE(header.find("#include \"Project/Handle.h\""), std::string::npos);
    EXPECT_NE(header.find("using Operations = ioj::native_soa::StorageOperations;"),
              std::string::npos);
    EXPECT_EQ(header.find("void apply_permutation("), std::string::npos);
    EXPECT_EQ(header.find("TArray"), std::string::npos);
    EXPECT_EQ(header.find("validate_array_sizes"), std::string::npos);
    EXPECT_NE(source.find("#include \"Project/Generated.h\""), std::string::npos);
}

TEST(Generator, LowersSoaFieldMaskFromAnnotatedMembers) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(context, R"(
        (module mask :header "Mask.h" :source "Mask.cpp"
          (struct FRows :field-mask-name FFieldMask :field-enum-name EField
            (member masks array FFieldMask)
            (member first array std::int32_t :mask-field true)
            (member matrix array std::int32_t :mask-field true :mask-dimensions ((row_index "2") (column_index "3")))
            (member last array std::int32_t :mask-field true))))")};

    auto const header{render_test_manifest(context, manifest).front().content};
    EXPECT_NE(header.find("enum class EField : std::uint8_t"), std::string::npos);
    EXPECT_NE(header.find("First = 0"), std::string::npos);
    EXPECT_NE(header.find("Matrix = static_cast<std::uint8_t>(First) + 1"), std::string::npos);
    EXPECT_NE(header.find("Last = static_cast<std::uint8_t>(Matrix) + (2) * (3)"),
              std::string::npos);
    EXPECT_NE(
        header.find(
            "struct FFieldMask : ioj::FieldMask<EField, static_cast<std::size_t>(EField::Count)>"),
        std::string::npos);
    EXPECT_NE(
        header.find("matrix_field(std::int32_t const row_index, std::int32_t const column_index)"),
        std::string::npos);
    EXPECT_NE(header.find("ColumnLayouts<FFieldMask, std::int32_t, std::int32_t, std::int32_t>"),
              std::string::npos);
    EXPECT_EQ(header.find("CoreMinimal.h"), std::string::npos);
}

class DuplicateOutputPaths : public testing::TestWithParam<char const*> {};

TEST_P(DuplicateOutputPaths, RejectsDuringRendering) {
    CompilerTestContext context;
    auto manifest{example_manifest(context)};
    auto duplicate{parse_test_manifest(
        context, std::format("(umbrella-module duplicate :header {} :headers ())", GetParam()))};
    manifest.modules.push_back(duplicate.modules.front());

    EXPECT_THROW(render_test_manifest(context, manifest), std::invalid_argument);
}

TEST_P(DuplicateOutputPaths, RejectsDuringLowering) {
    CompilerTestContext context;
    auto manifest{example_manifest(context)};
    auto duplicate{parse_test_manifest(
        context, std::format("(umbrella-module duplicate :header {} :headers ())", GetParam()))};
    manifest.modules.push_back(duplicate.modules.front());

    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(Generator,
                         DuplicateOutputPaths,
                         testing::Values("Generated.h", "generated.h"));

TEST(Generator, RendersDeterministically) {
    CompilerTestContext context;
    auto const modules{lower_test_manifest(context, example_manifest(context))};
    EXPECT_EQ(render_modules(context.ir, modules).front().content,
              render_modules(context.ir, modules).front().content);
}

TEST(Generator, WritesAndChecksOutputRoots) {
    auto const directory{std::filesystem::temp_directory_path() / "lispb-generation-test"};
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    auto const files{std::vector<GeneratedFile>{{"Generated/Value.h", "value\n"}}};

    EXPECT_EQ(generate_files(files, directory, directory, false), 0);
    EXPECT_EQ(generate_files(files, directory, directory, true), 0);
    {
        std::ofstream changed{directory / "Generated/Value.h"};
        changed << "stale\n";
    }
    EXPECT_EQ(generate_files(files, directory, directory, true), 1);
    std::filesystem::remove_all(directory, ignored);
}

TEST(Generator, ReportsMissingGeneratedFilesAsStale) {
    auto const directory{std::filesystem::temp_directory_path() / "lispb-missing-generation-test"};
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);

    EXPECT_EQ(generate_files(
                  {GeneratedFile{"Generated/Missing.h", "content\n"}}, directory, directory, true),
              1);
}

class InvalidDirectOutputPaths : public testing::TestWithParam<std::vector<GeneratedFile>> {};

TEST_P(InvalidDirectOutputPaths, RejectsPaths) {
    auto const directory{std::filesystem::temp_directory_path() /
                         "lispb-direct-output-validation-test"};
    EXPECT_THROW(generate_files(GetParam(), directory, directory, true), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(OutsideOutputRoot,
                         InvalidDirectOutputPaths,
                         testing::Values(std::vector<GeneratedFile>{{"../escape.h", "content\n"}},
                                         std::vector<GeneratedFile>{
                                             {std::filesystem::temp_directory_path() / "escape.h",
                                              "content\n"}}));

INSTANTIATE_TEST_SUITE_P(DuplicateAndReservedPaths,
                         InvalidDirectOutputPaths,
                         testing::Values(std::vector<GeneratedFile>{{"Generated/Value.h", "first"},
                                                                    {"generated/value.h",
                                                                     "second"}},
                                         std::vector<GeneratedFile>{{".lispb-outputs", "value"}}));

TEST(Generator, TracksDetectsAndRemovesOrphanedGeneratedFiles) {
    auto const directory{std::filesystem::temp_directory_path() / "lispb-orphan-generation-test"};
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    auto const generated_content{"// This file is autogenerated. Do not edit by hand.\nvalue\n"};
    auto const initial{std::vector<GeneratedFile>{
        {"Generated/First.h", generated_content},
        {"Generated/Second.h", generated_content},
    }};
    auto const remaining{std::vector<GeneratedFile>{{"Generated/First.h", generated_content}}};

    ASSERT_EQ(generate_files(initial, directory, directory, false), 0);
    EXPECT_TRUE(std::filesystem::exists(directory / ".lispb-outputs"));
    EXPECT_EQ(generate_files(remaining, directory, directory, true), 1);
    EXPECT_TRUE(std::filesystem::exists(directory / "Generated/Second.h"));

    EXPECT_EQ(generate_files(remaining, directory, directory, false), 0);
    EXPECT_FALSE(std::filesystem::exists(directory / "Generated/Second.h"));
    EXPECT_EQ(generate_files(remaining, directory, directory, true), 0);
    std::filesystem::remove_all(directory, ignored);
}

TEST(Generator, RefusesToRemoveModifiedOrphanedFiles) {
    auto const directory{std::filesystem::temp_directory_path() / "lispb-modified-orphan-test"};
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    auto const generated_content{"// This file is autogenerated. Do not edit by hand.\nvalue\n"};
    auto const initial{std::vector<GeneratedFile>{
        {"Generated/Keep.h", generated_content},
        {"Generated/Modified.h", generated_content},
    }};
    ASSERT_EQ(generate_files(initial, directory, directory, false), 0);
    {
        std::ofstream modified{directory / "Generated/Modified.h", std::ios::binary};
        modified << "hand-written\n";
    }

    EXPECT_THROW(
        generate_files(
            {GeneratedFile{"Generated/Keep.h", generated_content}}, directory, directory, false),
        std::runtime_error);
    EXPECT_TRUE(std::filesystem::exists(directory / "Generated/Modified.h"));
    std::filesystem::remove_all(directory, ignored);
}

TEST(Generator, PreservesDestinationWhenAtomicReplacementFails) {
    auto const directory{std::filesystem::temp_directory_path() / "lispb-atomic-write-test"};
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::filesystem::create_directories(directory / "Generated/Value.h");

    EXPECT_THROW(
        generate_files(
            {GeneratedFile{"Generated/Value.h", "replacement\n"}}, directory, directory, false),
        std::filesystem::filesystem_error);
    EXPECT_TRUE(std::filesystem::is_directory(directory / "Generated/Value.h"));
    EXPECT_FALSE(std::filesystem::exists(directory / "Generated/Value.h.lispb.tmp"));
    std::filesystem::remove_all(directory, ignored);
}

TEST(Generator, RejectsOutputThroughDirectorySymlinks) {
    auto const base{std::filesystem::temp_directory_path() / "lispb-symlink-containment-test"};
    auto const output_root{base / "output"};
    auto const outside{base / "outside"};
    std::error_code ignored;
    std::filesystem::remove_all(base, ignored);
    std::filesystem::create_directories(output_root);
    std::filesystem::create_directories(outside);
    std::error_code link_error;
    std::filesystem::create_directory_symlink(outside, output_root / "linked", link_error);
    if (link_error) {
        std::filesystem::remove_all(base, ignored);
        GTEST_SKIP() << "Directory symlinks are unavailable: " << link_error.message();
    }

    EXPECT_THROW(
        generate_files(
            {GeneratedFile{"linked/Escape.h", "content\n"}}, output_root, output_root, false),
        std::invalid_argument);
    EXPECT_FALSE(std::filesystem::exists(outside / "Escape.h"));
    std::filesystem::remove_all(base, ignored);
}

TEST(Generator, MapsAbsoluteProjectPathsIntoASeparateOutputRoot) {
    auto const base{std::filesystem::temp_directory_path() / "lispb-absolute-project-path-test"};
    auto const project_root{base / "project"};
    auto const output_root{base / "output"};
    std::error_code ignored;
    std::filesystem::remove_all(base, ignored);

    EXPECT_EQ(generate_files({GeneratedFile{project_root / "Generated/Value.h", "content\n"}},
                             project_root,
                             output_root,
                             false),
              0);
    EXPECT_TRUE(std::filesystem::exists(output_root / "Generated/Value.h"));
    std::filesystem::remove_all(base, ignored);
}

TEST(Generator, LowersFacadeWithPrivateBindingAndSourceDefinitions) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(
        context,
        R"(
        (module facade :header "Facade.h" :source "Facade.cpp" :header-include "Project/Facade.h"
          (facade FFacade @target target :validation ("check(target != nullptr);")
            :validation-dependencies (check) :bind-access private :friends (FOwner) :definitions-in-source true
            (method get int32 (parameter index int32)))))",
        {{"target",
          context.storage.cpp_types.add(
              "FTarget", {context.storage.cpp_types.dependency("FTarget", "Project/Target.h")})}})};

    manifest.types.add(
        "check",
        context.storage.cpp_types.add(
            "check", {context.storage.cpp_types.dependency("check", "CoreMinimal.h")}));

    auto const files{render_test_manifest(context, manifest)};
    ASSERT_EQ(files.size(), 2);
    EXPECT_NE(files[0].content.find("friend class FOwner;"), std::string::npos);
    EXPECT_NE(files[0].content.find("FTarget* target{nullptr};"), std::string::npos);
    EXPECT_NE(files[1].content.find("void FFacade::bind(FTarget& new_target)"), std::string::npos);
    EXPECT_NE(files[1].content.find("return target->get(index);"), std::string::npos);

    auto& store{context.storage.declarations};
    auto module{manifest.modules.front()};
    auto facade{store.items(store.normal_modules.declarations[module]).front()};
    store.facades.friend_kind[facade] = RecordKind::struct_;
    store.normal_modules.declarations[module] = store.range({facade});
    manifest.modules.front() = module;
    auto const struct_files{render_test_manifest(context, manifest)};
    EXPECT_NE(struct_files[0].content.find("friend struct FOwner;"), std::string::npos);
}

TEST(Generator, LowersFacadeWithReferenceTarget) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(
        context,
        R"(
        (module facade :header "Facade.h"
          (facade FFacade @target target :target-storage reference (method get int32))))",
        {{"target",
          context.storage.cpp_types.add(
              "FTarget", {context.storage.cpp_types.dependency("FTarget", "Project/Target.h")})}})};

    auto const files{render_test_manifest(context, manifest)};
    ASSERT_EQ(files.size(), 1);
    EXPECT_NE(files[0].content.find("FFacade(FTarget& new_target)"), std::string::npos);
    EXPECT_NE(files[0].content.find(": target{new_target}"), std::string::npos);
    EXPECT_NE(files[0].content.find("return target.get();"), std::string::npos);
    EXPECT_NE(files[0].content.find("FTarget& target;"), std::string::npos);
    EXPECT_EQ(files[0].content.find("bind("), std::string::npos);
}

TEST(Generator, AppliesFacadePreludeSettings) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(context, R"(
        (module facade :header "Facade.h" :prelude ("class FFacadeForward;")
          (facade FFacade FTarget target (method reset void))))")};

    auto const files{render_test_manifest(context, manifest)};
    ASSERT_EQ(files.size(), 1);
    EXPECT_NE(files[0].content.find("class FFacadeForward;"), std::string::npos);
}

TEST(Generator, AppliesUmbrellaPreludeSettings) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(context, R"(
        (umbrella-module all :header "All.h" :prelude ("#define PROJECT_INCLUDED 1") :headers ("Project/First.h" "Project/Second.h")))")};

    auto const files{render_test_manifest(context, manifest)};
    ASSERT_EQ(files.size(), 1);
    EXPECT_NE(files[0].content.find("#define PROJECT_INCLUDED 1"), std::string::npos);
    EXPECT_NE(files[0].content.find("#include \"Project/First.h\""), std::string::npos);
}

TEST(Generator, CommittedProductionFilesAreCurrent) {
    CompilerTestContext context;
    auto const project_root{
        std::filesystem::path{IOJ_CODEGEN_SOURCE_DIR}.parent_path().parent_path()};
    auto const project{
        lispb::load_project(context.storage.syntax, project_root / "lispb/project.lispb")};
    auto const target{*project.find_target("sandbox-code")};
    std::vector<std::filesystem::path> sources;
    for (auto const& source : project.sources(target)) {
        sources.push_back(project.root / source);
    }
    auto manifest{load_sources(context.storage, project.root / project.types(target), sources)};
    auto const files{render_test_manifest(context, manifest)};

    EXPECT_EQ(generate_files(files, project.root, project.root, true), 0);
}

TEST(Generator, AcceptsCrLfOutputInventory) {
    auto const directory{std::filesystem::temp_directory_path() / "lispb-crlf-inventory-test"};
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    auto const files{std::vector<GeneratedFile>{{"Generated/Value.h", "value\n"}}};

    ASSERT_EQ(generate_files(files, directory, directory, false), 0);
    {
        std::ofstream inventory{directory / ".lispb-outputs", std::ios::binary};
        inventory << "lispb-outputs-v1\r\nGenerated/Value.h\r\n";
    }
    EXPECT_EQ(generate_files(files, directory, directory, true), 0);
    std::filesystem::remove_all(directory, ignored);
}

TEST(Generator, FormatsOptedInOutputBeforeWritingAndChecking) {
    auto const directory{std::filesystem::temp_directory_path() /
                         "sandbox codegen formatting test"};
    std::filesystem::create_directories(directory);
    {
        std::ofstream style{directory / ".clang-format"};
        style << "BasedOnStyle: LLVM\nIndentWidth: 4\nLineEnding: CRLF\n";
    }
    auto const files{
        std::vector<GeneratedFile>{{"Generated/Value.h", "struct Value{int number;};\n", true}}};
    ASSERT_EQ(generate_files(files, directory, directory, false), 0);
    auto const destination{directory / "Generated/Value.h"};
    auto read = [&] {
        std::ifstream input{destination, std::ios::binary};
        return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    };
    EXPECT_NE(read().find("    int number;"), std::string::npos);
    EXPECT_EQ(read().find('\r'), std::string::npos);
    auto const timestamp{std::filesystem::last_write_time(destination)};
    EXPECT_EQ(generate_files(files, directory, directory, true), 0);
    EXPECT_EQ(std::filesystem::last_write_time(destination), timestamp);
    {
        std::ofstream output{destination, std::ios::binary};
        output << files.front().content;
    }
    EXPECT_EQ(generate_files(files, directory, directory, true), 1);
    EXPECT_EQ(read(), files.front().content);
    {
        std::ofstream style{directory / ".clang-format"};
        style << "InvalidFormatterOption: true\n";
    }
    EXPECT_THROW(generate_files(files, directory, directory, false), std::runtime_error);
    EXPECT_EQ(read(), files.front().content);
    std::filesystem::remove(destination);
    std::filesystem::remove(directory / "Generated");
    std::filesystem::remove(directory / ".clang-format");
    std::filesystem::remove(directory / ".lispb-outputs");
    std::filesystem::remove(directory);
}

} // namespace
} // namespace lispb::codegen
