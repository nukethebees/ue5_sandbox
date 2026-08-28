#include <codegen/generator.h>
#include <codegen/json.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

namespace codegen {
namespace {

auto example_manifest() -> Manifest {
    CppType handle{"FHandle", "Project/Handle.h"};
    handle.member_operations.emplace(TypeOperation::remove_at_swap, "remove_at_swap");
    return Manifest{
        .schema_version = 1,
        .types = {{"handle", std::move(handle)}},
        .modules = {
            SoaModuleSchema{
                .settings = ModuleSettings{
                    .name = "example",
                    .header = "Generated.h",
                    .source = "Generated.cpp",
                    .header_include = "Project/Generated.h",
                    .namespace_name = "example",
                    .include_order = {"Project/", "SandboxCore/"},
                },
                .structs = {
                    SoaSchema{
                        .name = "FData",
                        .members = {
                            SoaMemberSchema{"handles", SoaMemberKind::array, TypeRef{"@handle"}},
                            SoaMemberSchema{"weights", SoaMemberKind::array, TypeRef{"float"}},
                            SoaMemberSchema{
                                "nested_handles", SoaMemberKind::nested, TypeRef{"@handle"}},
                        },
                        .operations = all_storage_operations(),
                        .export_specifier = "EXAMPLE_API",
                    },
                },
            },
        },
    };
}

TEST(Generator, LowersDynamicSoaIntoTypedHeaderAndSource) {
    auto const files{render_modules(lower_modules(example_manifest()))};
    ASSERT_EQ(files.size(), 2);
    auto const& header{files.front().content};
    auto const& source{files.back().content};

    EXPECT_NE(header.find("struct EXAMPLE_API FDataConstView"), std::string::npos);
    EXPECT_NE(header.find("TConstArrayView<FHandle> handles;"), std::string::npos);
    EXPECT_NE(header.find("TArray<float> weights;"), std::string::npos);
    EXPECT_NE(header.find("handles.RemoveAtSwap(index, count, allow_shrinking);"),
              std::string::npos);
    EXPECT_NE(header.find("weights.RemoveAtSwap(index, count, allow_shrinking);"),
              std::string::npos);
    EXPECT_NE(header.find("nested_handles.remove_at_swap(index, count, allow_shrinking);"),
              std::string::npos);
    EXPECT_NE(header.find("template <typename Other>"), std::string::npos);
    EXPECT_NE(header.find("ml::SupportsApplyArrayPairsWith<FData, Other>"),
              std::string::npos);
    EXPECT_NE(header.find("#include \"Project/Handle.h\""), std::string::npos);
    EXPECT_EQ(header.find("SandboxCore/soa_permutation.h"), std::string::npos);

    EXPECT_NE(source.find("#include \"Project/Generated.h\""), std::string::npos);
    EXPECT_NE(source.find("SandboxCore/soa_permutation.h"), std::string::npos);
    EXPECT_NE(source.find("namespace example {"), std::string::npos);
    EXPECT_NE(source.find("void FData::reset()"), std::string::npos);
    EXPECT_NE(source.find("ml::apply_permutation(handles, indices);"), std::string::npos);
    EXPECT_NE(source.find("auto FData::get_view() -> View"), std::string::npos);
}

TEST(Generator, RejectsDuplicateOutputPaths) {
    auto manifest{example_manifest()};
    manifest.modules.push_back(UmbrellaModuleSchema{
        .settings = ModuleSettings{.name = "duplicate", .header = "Generated.h"},
        .headers = {},
    });

    EXPECT_THROW(render_modules(lower_modules(manifest)), std::invalid_argument);
}

TEST(Generator, RejectsCaseInsensitiveDuplicateOutputPaths) {
    auto manifest{example_manifest()};
    manifest.modules.push_back(UmbrellaModuleSchema{
        .settings = ModuleSettings{.name = "duplicate", .header = "generated.h"},
        .headers = {},
    });

    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
}

TEST(Generator, RejectsInvalidVectorDimensions) {
    Manifest const manifest{
        .schema_version = 1,
        .modules = {VectorModuleSchema{
            .settings = ModuleSettings{.name = "vectors", .header = "Vectors.h"},
            .storage_name = "FVectors",
            .value_type = TypeRef{"float"},
            .components = {},
            .equivalent_type = TypeRef{"FVector"},
        }},
    };

    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
}

TEST(Generator, RejectsDuplicateStorageOperations) {
    auto manifest{example_manifest()};
    auto& module{std::get<SoaModuleSchema>(manifest.modules.front())};
    module.structs.front().operations.push_back(StorageOperation::reset);

    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
}

TEST(Generator, RejectsPartiallyEquivalentHomogeneousLayouts) {
    Manifest const manifest{
        .schema_version = 1,
        .modules = {HomogeneousModuleSchema{
            .settings = ModuleSettings{
                .name = "vectors", .header = "Vectors.h", .source = "Vectors.cpp"},
            .layouts = {HomogeneousLayoutSchema{
                .name = "Vectors",
                .components = {"xs", "ys"},
                .value_types = {
                    HomogeneousValueSchema{
                        .type = TypeRef{"float"},
                        .suffix = "f",
                        .equivalent_type = TypeRef{"FVector2f"},
                    },
                    HomogeneousValueSchema{
                        .type = TypeRef{"double"},
                        .suffix = "d",
                    },
                },
            }},
        }},
    };

    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
}

TEST(Generator, RendersDeterministically) {
    auto const modules{lower_modules(example_manifest())};
    EXPECT_EQ(render_modules(modules).front().content,
              render_modules(modules).front().content);
}

TEST(Generator, WritesAndChecksOutputRoots) {
    auto const directory{std::filesystem::temp_directory_path() /
                         "sandbox-codegen-generation-test"};
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
    auto const directory{std::filesystem::temp_directory_path() /
                         "sandbox-codegen-missing-generation-test"};
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);

    EXPECT_EQ(generate_files({GeneratedFile{"Generated/Missing.h", "content\n"}},
                             directory,
                             directory,
                             true),
              1);
}

TEST(Generator, RejectsOutputPathsOutsideOutputRoot) {
    auto const directory{std::filesystem::temp_directory_path() /
                         "sandbox-codegen-output-containment-test"};

    EXPECT_THROW(generate_files({GeneratedFile{"../escape.h", "content\n"}},
                                directory,
                                directory,
                                true),
                 std::invalid_argument);
    EXPECT_THROW(generate_files(
                     {GeneratedFile{directory.parent_path() / "escape.h", "content\n"}},
                     directory,
                     directory,
                     true),
                 std::invalid_argument);
}

TEST(Generator, LowersFacadeWithPrivateBindingAndSourceDefinitions) {
    auto manifest{Manifest{
        .schema_version = 1,
        .types = {{"target", CppType{"FTarget", "Project/Target.h"}}},
        .modules = {FacadeModuleSchema{
            .settings = ModuleSettings{
                .name = "facade",
                .header = "Facade.h",
                .source = "Facade.cpp",
                .header_include = "Project/Facade.h",
            },
            .facade = FacadeSchema{
                .name = "FFacade",
                .target_type = TypeRef{"@target"},
                .target_member_name = "target",
                .methods = {FacadeMethodSchema{
                    .name = "get",
                    .return_type = TypeRef{"int32"},
                    .parameters = {ParameterSchema{TypeRef{"int32"}, "index"}},
                }},
                .validation_lines = {"check(target != nullptr);"},
                .validation_dependencies = {"check"},
                .bind_access = "private",
                .friends = {"FOwner"},
                .definitions_in_source = true,
            },
        }},
    }};
    manifest.types.emplace("check", CppType{"check", "CoreMinimal.h"});

    auto const files{render_modules(lower_modules(manifest))};
    ASSERT_EQ(files.size(), 2);
    EXPECT_NE(files[0].content.find("friend class FOwner;"), std::string::npos);
    EXPECT_NE(files[0].content.find("FTarget* target{nullptr};"), std::string::npos);
    EXPECT_NE(files[1].content.find("void FFacade::bind(FTarget& new_target)"),
              std::string::npos);
    EXPECT_NE(files[1].content.find("return target->get(index);"), std::string::npos);
}

TEST(Generator, LowersHomogeneousLayouts) {
    Manifest const manifest{
        .schema_version = 1,
        .modules = {HomogeneousModuleSchema{
            .settings = ModuleSettings{.name = "rotators",
                                       .header = "Rotators.h",
                                       .source = "Rotators.cpp"},
            .layouts = {HomogeneousLayoutSchema{
                .name = "Rotators",
                .components = {"pitches", "yaws", "rolls"},
                .value_types = {HomogeneousValueSchema{TypeRef{"float"}, "f"}},
                .export_specifier = "EXAMPLE_API",
            }},
        }},
    };

    auto const files{render_modules(lower_modules(manifest))};
    ASSERT_EQ(files.size(), 2);
    EXPECT_NE(files[0].content.find("template <typename T>\nstruct TRotatorsView"),
              std::string::npos);
    EXPECT_NE(files[0].content.find("struct EXAMPLE_API FRotatorsf"), std::string::npos);
    EXPECT_NE(files[1].content.find("void FRotatorsf::apply_permutation"),
              std::string::npos);
}

TEST(Generator, LowersVectorLayoutsThroughDynamicSoa) {
    Manifest const manifest{
        .schema_version = 1,
        .types = {{"vector", CppType{"FVector3f", "CoreMinimal.h"}}},
        .modules = {VectorModuleSchema{
            .settings = ModuleSettings{.name = "vectors",
                                       .header = "Vectors.h",
                                       .source = "Vectors.cpp"},
            .storage_name = "FVectors3f",
            .value_type = TypeRef{"float"},
            .components = {"xs", "ys", "zs"},
            .equivalent_type = TypeRef{"@vector"},
            .export_specifier = "EXAMPLE_API",
        }},
    };

    auto const files{render_modules(lower_modules(manifest))};
    ASSERT_EQ(files.size(), 2);
    EXPECT_NE(files[0].content.find("struct Data"), std::string::npos);
    EXPECT_NE(files[0].content.find("FVector3f"), std::string::npos);
    EXPECT_NE(files[0].content.find("float* xs;"), std::string::npos);
    EXPECT_EQ(files[0].content.find("value_type* xs;"), std::string::npos);
    EXPECT_NE(files[0].content.find("auto at(int32 const index) const -> FVector3f {\n"
                                    "        validate_array_sizes();"),
              std::string::npos);
    EXPECT_NE(files[0].content.find("auto add(value_type const x"), std::string::npos);
    EXPECT_NE(files[0].content.find("return add(value.X, value.Y, value.Z);"),
              std::string::npos);
}

TEST(Generator, AppliesVectorNamespaceAndPreludeSettings) {
    Manifest const manifest{
        .schema_version = 1,
        .modules = {VectorModuleSchema{
            .settings = ModuleSettings{
                .name = "vectors",
                .header = "Vectors.h",
                .source = "Vectors.cpp",
                .namespace_name = "project::vectors",
                .prelude_lines = {"class FVectorForward;"},
            },
            .storage_name = "FVectors",
            .value_type = TypeRef{"float"},
            .components = {"xs", "ys"},
            .equivalent_type = TypeRef{"FVector2f"},
        }},
    };

    auto const files{render_modules(lower_modules(manifest))};
    ASSERT_EQ(files.size(), 2);
    EXPECT_NE(files[0].content.find("class FVectorForward;"), std::string::npos);
    EXPECT_NE(files[0].content.find("namespace project::vectors {"), std::string::npos);
    EXPECT_NE(files[1].content.find("namespace project::vectors {"), std::string::npos);
}

TEST(Generator, AppliesFacadePreludeSettings) {
    auto manifest{Manifest{
        .schema_version = 1,
        .modules = {FacadeModuleSchema{
            .settings = ModuleSettings{
                .name = "facade",
                .header = "Facade.h",
                .prelude_lines = {"class FFacadeForward;"},
            },
            .facade = FacadeSchema{
                .name = "FFacade",
                .target_type = TypeRef{"FTarget"},
                .target_member_name = "target",
                .methods = {FacadeMethodSchema{
                    .name = "reset",
                    .return_type = TypeRef{"void"},
                }},
            },
        }},
    }};

    auto const files{render_modules(lower_modules(manifest))};
    ASSERT_EQ(files.size(), 1);
    EXPECT_NE(files[0].content.find("class FFacadeForward;"), std::string::npos);
}

TEST(Generator, AppliesUmbrellaPreludeSettings) {
    Manifest const manifest{
        .schema_version = 1,
        .modules = {UmbrellaModuleSchema{
            .settings = ModuleSettings{
                .name = "all",
                .header = "All.h",
                .prelude_lines = {"#define PROJECT_INCLUDED 1"},
            },
            .headers = {"Project/First.h", "Project/Second.h"},
        }},
    };

    auto const files{render_modules(lower_modules(manifest))};
    ASSERT_EQ(files.size(), 1);
    EXPECT_NE(files[0].content.find("#define PROJECT_INCLUDED 1"), std::string::npos);
    EXPECT_NE(files[0].content.find("#include \"Project/First.h\""), std::string::npos);
}

TEST(Generator, LowersFlatAndNestedFixedSoaLayouts) {
    CppType child_type{"FChild"};
    child_type.member_operations.emplace(TypeOperation::remove_at_swap, "remove_at_swap");
    Manifest const manifest{
        .schema_version = 1,
        .types = {{"child", std::move(child_type)}},
        .modules = {SoaModuleSchema{
            .settings = ModuleSettings{
                .name = "fixed", .header = "Fixed.h", .source = "Fixed.cpp"},
            .structs = {
                SoaSchema{
                    .name = "FChild",
                    .members = {SoaMemberSchema{"values",
                                                SoaMemberKind::array,
                                                TypeRef{"float"}}},
                    .operations = all_storage_operations(),
                    .fixed = FixedSoaSchema{"TChildStorage", {}},
                },
                SoaSchema{
                    .name = "FRows",
                    .members = {
                        SoaMemberSchema{"ids", SoaMemberKind::array, TypeRef{"int32"}},
                        SoaMemberSchema{"children",
                                        SoaMemberKind::nested,
                                        TypeRef{"@child"},
                                        "FChild"},
                    },
                    .operations = all_storage_operations(),
                    .fixed = FixedSoaSchema{"TRowsStorage", {"TFixedRows"}},
                },
            },
        }},
    };

    auto const files{render_modules(lower_modules(manifest))};
    ASSERT_EQ(files.size(), 2);
    auto const& header{files.front().content};
    EXPECT_NE(header.find("struct TChildStorage"), std::string::npos);
    EXPECT_NE(header.find("TChildStorage<Capacity> children_;"), std::string::npos);
    EXPECT_NE(header.find("struct TFixedRows"), std::string::npos);
    EXPECT_NE(header.find("new_children_values"), std::string::npos);
    EXPECT_NE(header.find("storage_.construct_from_view_at"), std::string::npos);
}

TEST(Generator, RendersCompleteProductionManifest) {
    auto const manifest_path{std::filesystem::path{SANDBOX_CODEGEN_SOURCE_DIR} /
                             "manifests/manifest.json"};
    auto const manifest{load_manifest(manifest_path)};
    auto const files{render_modules(lower_modules(manifest))};

    EXPECT_EQ(files.size(), 64);
    EXPECT_EQ(files.front().path,
              "Plugins/SandboxCore/Source/SandboxCore/Public/SandboxCore/countdown_timers.h");
    EXPECT_EQ(files.back().path,
              "Plugins/SpaceGame/Source/SpaceGame/Private/defences/spinners/"
              "TestTubeSpinnersPhaseInterface.cpp");
    EXPECT_EQ(files, render_modules(lower_modules(manifest)));
}

TEST(Generator, CommittedProductionFilesAreCurrent) {
    auto const codegen_root{std::filesystem::path{SANDBOX_CODEGEN_SOURCE_DIR}};
    auto const project_root{codegen_root.parent_path()};
    auto const manifest{load_manifest(codegen_root / "manifests/manifest.json")};
    auto const files{render_modules(lower_modules(manifest))};

    for (auto const& file : files) {
        auto const path{project_root / file.path};
        ASSERT_TRUE(std::filesystem::exists(path)) << path.string();
        std::ifstream input{path, std::ios::binary};
        ASSERT_TRUE(input) << path.string();
        auto const content{std::string{std::istreambuf_iterator<char>{input},
                                      std::istreambuf_iterator<char>{}}};
        EXPECT_EQ(content, file.content) << path.string();
    }
}

} // namespace
} // namespace codegen
