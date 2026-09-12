#include <kernel_codegen/compiler.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace kernel_codegen {
namespace {

constexpr std::string_view kernel_source = R"(
(kernel-module arithmetic
  (emit unreal
    (header "ArrayKernels.h")
    (source "ArrayKernels.cpp")
    (header-include "ArrayKernels.h")
    (namespace ml)
    (export COMPILE_FIXTURE_API))
  (type-set numeric float)
  (map multiply
    (types numeric)
    (operand lhs array)
    (operand rhs scalar)
    (output out)
    (expression (* lhs rhs))
    (variants
      (out-of-place multiply))))
)";

class TemporaryProject {
  public:
    explicit TemporaryProject(std::string_view const name)
        : root_{std::filesystem::temp_directory_path() /
                ("sandbox-kernel-codegen-" + std::string{name})} {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
        std::filesystem::create_directories(root_);
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

TEST(KernelCompiler, WritesChecksAndDetectsStaleOutputs) {
    TemporaryProject project{"check-mode"};
    project.write("manifest.sbxgen",
                  R"((kernel-manifest :schema-version 1 :entries ("array_math.sbxkernel")))");
    project.write("array_math.sbxkernel", kernel_source);
    auto const options{CompileOptions{.manifest = project.path("manifest.sbxgen"),
                                      .output_root = project.path("generated")}};

    ASSERT_EQ(compile_manifest(options), 0);
    EXPECT_TRUE(
        project.read("generated/ArrayKernels.h").contains("void COMPILE_FIXTURE_API multiply("));
    EXPECT_TRUE(project.read("generated/ArrayKernels.cpp").contains("lhs[i] * rhs"));
    EXPECT_TRUE(project.read("generated/.sandbox-codegen-outputs").contains("ArrayKernels.cpp"));
    EXPECT_EQ(compile_manifest(CompileOptions{
                  .manifest = options.manifest, .output_root = options.output_root, .check = true}),
              0);

    project.write("generated/ArrayKernels.h", "stale\n");
    EXPECT_EQ(compile_manifest(CompileOptions{
                  .manifest = options.manifest, .output_root = options.output_root, .check = true}),
              1);
}

TEST(KernelCompiler, RejectsOutputCollisionsAcrossManifestEntries) {
    TemporaryProject project{"duplicate-outputs"};
    project.write(
        "manifest.sbxgen",
        R"((kernel-manifest :schema-version 1 :entries ("first.sbxkernel" "second.sbxkernel")))");
    project.write("first.sbxkernel", kernel_source);
    project.write("second.sbxkernel", kernel_source);

    EXPECT_THROW(static_cast<void>(
                     compile_manifest(CompileOptions{.manifest = project.path("manifest.sbxgen"),
                                                     .output_root = project.path("generated")})),
                 std::invalid_argument);
}

TEST(KernelCompiler, EmitsOnlyTheSelectedProfile) {
    TemporaryProject project{"profile-selection"};
    project.write("manifest.sbxgen",
                  R"((kernel-manifest :schema-version 1 :entries ("array_math.sbxkernel")))");
    auto source{std::string{kernel_source}};
    auto const insertion{source.find("  (type-set")};
    source.insert(insertion,
                  "  (emit standard\n"
                  "    (header \"standard/Kernels.h\")\n"
                  "    (source \"standard/Kernels.cpp\")\n"
                  "    (tests \"standard/KernelsTests.cpp\")\n"
                  "    (header-include \"standard/Kernels.h\")\n"
                  "    (namespace ml))\n");
    project.write("array_math.sbxkernel", source);

    ASSERT_EQ(compile_manifest(CompileOptions{.manifest = project.path("manifest.sbxgen"),
                                              .output_root = project.path("generated"),
                                              .profile = Profile::standard}),
              0);
    EXPECT_TRUE(project.read("generated/standard/Kernels.h").contains("std::span<float const>"));
    EXPECT_TRUE(project.read("generated/standard/KernelsTests.cpp").contains("TEST("));
    EXPECT_FALSE(std::filesystem::exists(project.path("generated/ArrayKernels.h")));
}

TEST(KernelCompiler, EmitsOnlyTheSelectedAvx2LabKernel) {
    TemporaryProject project{"avx2-lab-profile"};
    project.write("manifest.sbxgen",
                  R"((kernel-manifest :schema-version 1 :entries ("array_math.sbxkernel")))");
    auto source{std::string{kernel_source}};
    source.replace(source.find("    (variants"), 0, "    (aliasing pairwise-disjoint)\n");
    auto const insertion{source.find("  (type-set")};
    source.insert(insertion,
                  "  (emit unreal-avx2-lab\n"
                  "    (header \"lab/Kernels.h\")\n"
                  "    (source \"lab/Kernels.cpp\")\n"
                  "    (header-include \"lab/Kernels.h\")\n"
                  "    (namespace ml::lab)\n"
                  "    (select\n"
                  "      (operation multiply)\n"
                  "      (type float)\n"
                  "      (storage array scalar)\n"
                  "      (variant out-of-place)))\n");
    project.write("array_math.sbxkernel", source);

    ASSERT_EQ(compile_manifest(CompileOptions{.manifest = project.path("manifest.sbxgen"),
                                              .output_root = project.path("generated"),
                                              .profile = Profile::unreal_avx2_lab}),
              0);
    EXPECT_TRUE(project.read("generated/lab/Kernels.h").contains("multiply_avx2"));
    EXPECT_TRUE(project.read("generated/lab/Kernels.cpp").contains("_mm256_mul_ps"));
    EXPECT_FALSE(std::filesystem::exists(project.path("generated/ArrayKernels.h")));
}

TEST(KernelCompiler, EmitsIsolatedNativeSimdLabSources) {
    TemporaryProject project{"native-simd-lab-profile"};
    project.write("manifest.sbxgen",
                  R"((kernel-manifest :schema-version 1 :entries ("array_math.sbxkernel")))");
    auto source{std::string{kernel_source}};
    source.replace(source.find("    (variants"), 0, "    (aliasing pairwise-disjoint)\n");
    auto const insertion{source.find("  (type-set")};
    source.insert(insertion,
                  "  (emit native-x86-simd-lab\n"
                  "    (header \"native/Kernels.h\")\n"
                  "    (source \"native/KernelsAvx2.cpp\")\n"
                  "    (avx512-source \"native/KernelsAvx512.cpp\")\n"
                  "    (dispatch-source \"native/KernelsDispatch.cpp\")\n"
                  "    (header-include \"native/Kernels.h\")\n"
                  "    (namespace ml::lab)\n"
                  "    (select\n"
                  "      (operation multiply)\n"
                  "      (type float)\n"
                  "      (storage array scalar)\n"
                  "      (variant out-of-place)))\n");
    project.write("array_math.sbxkernel", source);

    ASSERT_EQ(compile_manifest(CompileOptions{.manifest = project.path("manifest.sbxgen"),
                                              .output_root = project.path("generated"),
                                              .profile = Profile::native_x86_simd_lab}),
              0);
    EXPECT_TRUE(project.read("generated/native/Kernels.h").contains("X86SimdBackend"));
    EXPECT_TRUE(project.read("generated/native/KernelsAvx2.cpp").contains("_mm256_mul_ps"));
    EXPECT_TRUE(project.read("generated/native/KernelsAvx512.cpp").contains("_mm512_mul_ps"));
    EXPECT_TRUE(
        project.read("generated/native/KernelsDispatch.cpp").contains("cpu_features::GetX86Info"));
}

}
}
