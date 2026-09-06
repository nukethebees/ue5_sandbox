#pragma once

#include <codegen/sexpr/syntax.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace kernel_codegen::detail {

using codegen::sexpr::SourceSpan;

enum class StorageKind { array, scalar };
enum class Profile { unreal, standard, unreal_avx2_lab, native_x86_simd_lab };
enum class ExpressionKind { reference, literal, constant, binary };
enum class ConstantKind { nan, infinity, negative_infinity };
enum class OperationKind { map, sum };
enum class VariantKind { out_of_place, in_place, sum };
enum class Aliasing { output_disjoint, pairwise_disjoint };
enum class FloatingPointMode { strict, relaxed };

struct Expression {
    ExpressionKind kind;
    std::string value;
    std::vector<Expression> arguments;
    SourceSpan span;
    ConstantKind constant{ConstantKind::nan};
};

struct Operand {
    std::string name;
    std::vector<StorageKind> storage;
    SourceSpan span;
};

struct Variant {
    VariantKind kind;
    std::string public_name;
    std::optional<std::string> target;
    SourceSpan span;
};

struct Operation {
    OperationKind kind;
    std::string name;
    std::string type_set;
    std::vector<Operand> operands;
    std::string output;
    Expression expression;
    std::vector<Variant> variants;
    Aliasing aliasing{Aliasing::output_disjoint};
    std::vector<FloatingPointMode> floating_point_modes{FloatingPointMode::strict};
    SourceSpan span;
};

struct TypeSet {
    std::string name;
    std::vector<std::string> types;
    SourceSpan span;
};

struct VariantSelection {
    std::string operation;
    std::string type;
    std::vector<StorageKind> storage;
    VariantKind variant;
    SourceSpan span;
};

struct Emission {
    Profile profile;
    std::filesystem::path header;
    std::filesystem::path source;
    std::optional<std::filesystem::path> avx512_source;
    std::optional<std::filesystem::path> dispatch_source;
    std::optional<std::filesystem::path> relaxed_avx2_source;
    std::optional<std::filesystem::path> relaxed_avx512_source;
    std::optional<std::filesystem::path> tests;
    std::string header_include;
    std::string cpp_namespace;
    std::string export_specifier;
    std::optional<int> soaos_lanes;
    std::optional<VariantSelection> selection;
    SourceSpan span;
};

struct KernelModule {
    std::string name;
    std::vector<Emission> emissions;
    std::vector<TypeSet> type_sets;
    std::vector<Operation> operations;
    SourceSpan span;
};

struct Document {
    std::vector<KernelModule> modules;
};

struct ManifestEntry {
    std::filesystem::path input;
};

struct Manifest {
    std::vector<ManifestEntry> entries;
};

}
