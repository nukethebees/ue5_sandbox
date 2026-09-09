#include "fixed_soa_internal.h"
#include "lowering_utils.h"

#include <sstream>
#include <stdexcept>

namespace codegen::detail {

auto lower_native_soa(SoaSchema const& schema,
                      std::map<std::string, SoaSchema const*> const& schemas,
                      std::map<std::string, CppType> const& types) -> LoweredSoa {
    if (schema.fixed || !schema.functions.empty() || !schema.mutable_view_functions.empty() ||
        schema.equivalent_type || !schema.using_declarations.empty()) {
        throw std::invalid_argument{"Experimental stdlib SoA does not support custom functions, "
                                    "fixed storage or equivalent types"};
    }
    auto const layout{build_soa_layout(schema, schemas, types, false)};
    for (auto const& leaf : layout.leaves) {
        if (leaf.type.spelling == "bool") {
            throw std::invalid_argument{"Experimental stdlib SoA requires contiguous columns; "
                                        "std::vector<bool> is unsupported"};
        }
    }
    auto const view{schema.view_name.value_or(schema.name + "View")};
    auto const const_view{schema.const_view_name.value_or(schema.name + "ConstView")};
    std::vector<TypeDependency> dependencies{{"native_storage", "native_soa/storage.h", {}}};
    std::ostringstream out;
    for (bool const immutable : {true, false}) {
        out << "struct " << (immutable ? const_view : view) << " {\n";
        for (auto const& member : layout.members) {
            auto const& resolved{member.member};
            if (member.schema->kind == SoaMemberKind::array) {
                out << "std::span<" << resolved.element_type.spelling << (immutable ? " const" : "")
                    << "> " << member.schema->name << ";\n";
            } else {
                auto const& child{*schemas.at(*member.schema->nested_schema)};
                out << (immutable ? child.const_view_name.value_or(child.name + "ConstView")
                                  : child.view_name.value_or(child.name + "View"))
                    << " " << member.schema->name << ";\n";
            }
        }
        auto const& first{layout.leaves.front()};
        out << "auto num() const noexcept -> std::int32_t { return static_cast<std::int32_t>("
            << join(first.path, ".") << ".size()); }\n";
        out << "template <typename Fn> void each_column(Fn&& fn) const {\n";
        for (auto const& leaf : layout.leaves) {
            out << "fn(" << join(leaf.path, ".") << ");\n";
        }
        out << "}\n";
        auto const view_type{immutable ? const_view : view};
        out << "auto slice(std::int32_t offset, std::int32_t count) const -> " << view_type
            << " { ml::native_soa::require(offset >= 0 && count >= 0 && offset <= num() && count "
               "<= num() - offset); return {\n";
        for (auto const& member : layout.members) {
            out << member.schema->name
                << (member.schema->kind == SoaMemberKind::array
                        ? ".subspan(static_cast<std::size_t>(offset), "
                          "static_cast<std::size_t>(count))"
                        : ".slice(offset, count)")
                << ",\n";
        }
        out << "}; }\n};\n";
    }
    out << "struct " << schema.name << " {\nusing View = " << view
        << ";\nusing ConstView = " << const_view << ";\nusing size_type = std::int32_t;\n";
    for (auto const& member : layout.members) {
        auto const& type{member.member.element_type};
        dependencies.insert(dependencies.end(), type.dependencies.begin(), type.dependencies.end());
        out << (member.schema->kind == SoaMemberKind::array
                    ? "ml::native_soa::Vector<" + type.spelling + ">"
                    : type.spelling)
            << " " << member.schema->name << ";\n";
    }
    out << "auto num() const noexcept -> size_type { return static_cast<size_type>("
        << join(layout.leaves.front().path, ".") << ".size()); }\n";
    out << "template <typename Fn> void each_column(Fn&& fn) const {\n";
    for (auto const& leaf : layout.leaves) {
        out << "fn(" << join(leaf.path, ".") << ");\n";
    }
    out << "}\n";
    out << "void reserve(size_type const count) { ml::native_soa::require(count >= 0);\n";
    for (auto const& leaf : layout.leaves) {
        out << join(leaf.path, ".") << ".reserve(static_cast<std::size_t>(count));\n";
    }
    out << "}\nvoid reset() noexcept {\n";
    for (auto const& leaf : layout.leaves) {
        out << join(leaf.path, ".") << ".clear();\n";
    }
    out << "}\nvoid set_num(size_type const count) { ml::native_soa::require(count >= 0);\nauto "
           "const size{static_cast<std::size_t>(count)};\n";
    for (auto const& leaf : layout.leaves) {
        out << join(leaf.path, ".") << ".resize(size);\n";
    }
    out << "}\nvoid add_defaulted(size_type const count) { auto const old_num{num()}; "
           "ml::native_soa::require(count >= 0 && count <= std::numeric_limits<size_type>::max() - "
           "old_num); set_num(old_num + count); }\n";
    out << "void remove_at_swap(size_type const index, size_type const count) { auto const "
           "old_num{num()}; ml::native_soa::require(index >= 0 && index <= old_num && count >= 0 "
           "&& count <= old_num - index); auto const moved{std::min(count, old_num-index-count)}; "
           "auto const source{old_num-moved};\n";
    for (auto const& leaf : layout.leaves) {
        auto const column{join(leaf.path, ".")};
        out << "for (size_type i{}; i < moved; ++i) { " << column << "[index+i] = " << column
            << "[source+i]; }\n";
    }
    out << "set_num(old_num-count); }\n";
    out << "void append_from(ConstView source) { auto const count{source.num()};\n"
        << "ml::native_soa::require(count <= std::numeric_limits<size_type>::max() - num());\n"
        << "source.each_column([&](auto column) { ml::native_soa::require(column.size() == "
           "static_cast<std::size_t>(count)); });\n"
        << "if (count == 0) { return; }\n";
    for (auto const& leaf : layout.leaves) {
        auto const column{join(leaf.path, ".")};
        out << "{ auto const address{reinterpret_cast<std::uintptr_t>(source." << column
            << ".data())};\n"
            << "auto const begin{reinterpret_cast<std::uintptr_t>(" << column << ".data())};\n"
            << "ml::native_soa::require(address < begin || address >= begin + " << column
            << ".size() * sizeof(" << leaf.type.spelling << ")); }\n";
    }
    for (auto const& leaf : layout.leaves) {
        auto const column{join(leaf.path, ".")};
        out << column << ".insert(" << column << ".end(), source." << column << ".begin(), source."
            << column << ".end());\n";
    }
    out << "}\n";
    for (bool const immutable : {false, true}) {
        out << "auto get_view()" << (immutable ? " const" : "") << " -> "
            << (immutable ? "ConstView" : "View") << " { return {\n";
        for (auto const& member : layout.members) {
            out << member.schema->name;
            if (member.schema->kind == SoaMemberKind::nested) {
                out << ".get_view()";
            }
            out << ",\n";
        }
        out << "}; }\n";
    }
    out << "};\n";
    return LoweredSoa{{raw(out.str(), std::move(dependencies))}, {}};
}

}
