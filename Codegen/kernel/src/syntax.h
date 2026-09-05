#pragma once

#include <codegen/sexpr/syntax.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace kernel_codegen::detail {

using codegen::sexpr::SourceSpan;

enum class StorageKind { array, scalar };
enum class ExpressionKind { reference, literal, constant, binary };
enum class ConstantKind { nan, infinity, negative_infinity };
enum class VariantKind { out_of_place, in_place };
enum class Aliasing { output_disjoint, pairwise_disjoint };

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

struct MapOperation {
    std::string name;
    std::string type_set;
    std::vector<Operand> operands;
    std::string output;
    Expression expression;
    std::vector<Variant> variants;
    Aliasing aliasing{Aliasing::output_disjoint};
    SourceSpan span;
};

struct TypeSet {
    std::string name;
    std::vector<std::string> types;
    SourceSpan span;
};

struct KernelModule {
    std::string name;
    std::filesystem::path header;
    std::filesystem::path source;
    std::string header_include;
    std::string cpp_namespace;
    std::string export_specifier;
    std::vector<TypeSet> type_sets;
    std::vector<MapOperation> operations;
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
