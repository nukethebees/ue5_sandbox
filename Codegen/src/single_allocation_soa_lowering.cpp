#include "fixed_soa_internal.h"
#include "lowering_utils.h"

#include <set>
#include <sstream>
#include <stdexcept>

namespace codegen::detail {

static auto
    single_allocation_view_expression(SoaSchema const& schema,
                                      std::map<std::string, SoaSchema const*> const& schemas,
                                      std::vector<std::string> const& prefix,
                                      bool const is_const) -> std::string {
    auto const view_name{is_const ? schema.const_view_name.value_or(schema.name + "ConstView")
                                  : schema.view_name.value_or(schema.name + "View")};
    std::vector<std::string> values;
    for (auto const& member : schema.members) {
        auto path{prefix};
        path.push_back(member.name);
        if (member.kind == SoaMemberKind::nested) {
            values.push_back(single_allocation_view_expression(
                *schemas.at(*member.nested_schema), schemas, path, is_const));
        } else {
            values.push_back("{columns." + join(path, "_") + ", count}");
        }
    }
    return view_name + "{" + join(values, ", ") + "}";
}

auto lower_single_allocation_node(SoaSchema const& schema,
                                  std::map<std::string, SoaSchema const*> const& schemas,
                                  std::map<std::string, CppType> const& types) -> Node {
    auto const layout{build_soa_layout(schema, schemas, types, false)};
    auto const& name{*schema.experimental_single_allocation};
    auto const storage_name{name + "Storage"};
    std::vector<TypeDependency> dependencies{
        {"single_allocation_storage", "SbxCoreExperiments/single_allocation_storage.h", {}}};
    std::set<std::string> names;
    std::map<std::string, std::string> type_ids;
    std::vector<FixedLeaf const*> unique_types;
    for (auto const& leaf : layout.leaves) {
        if (type_ids.emplace(leaf.type.spelling, fixed_leaf_argument(leaf)).second) {
            unique_types.push_back(&leaf);
        }
    }
    auto emit_copy_sizes = [&](std::ostringstream& output, std::string const& count) {
        for (auto const* leaf : unique_types) {
            output << "        auto const " << fixed_leaf_argument(*leaf) << "_bytes{" << count
                   << " * sizeof(" << leaf->type.spelling << ")};\n";
        }
    };
    std::ostringstream out;
    out << "struct " << storage_name << " : ml::single_allocation_experiment::StorageOperations {\n"
        << "using View = " << schema.view_name.value_or(schema.name + "View") << ";\n"
        << "using ConstView = " << schema.const_view_name.value_or(schema.name + "ConstView")
        << ";\n"
        << "using size_type = int32;\nusing byte_size_type = SIZE_T;\n\n"
        << "inline static constexpr byte_size_type "
           "max_allocation_size{std::numeric_limits<byte_size_type>::max()};\n"
        << "inline static constexpr size_type capacity_granularity{64};\n\n";
    std::vector<std::string> alignments;
    for (auto const* leaf : unique_types) {
        auto const id{fixed_leaf_argument(*leaf)};
        auto const& type{leaf->type.spelling};
        out << "static_assert(ml::single_allocation_experiment::supported_leaf<" << type
            << ">, \"Single-allocation leaf " << join(leaf->path, ".")
            << " requires a non-cv, trivially copyable/copy-constructible/destructible, nothrow "
               "default-constructible object type.\");\n"
            << "inline static constexpr byte_size_type " << id << "_alignment{alignof(" << type
            << ") > 64 ? alignof(" << type << ") : 64};\n\n";
        alignments.push_back(id + "_alignment");
    }
    out << "inline static constexpr byte_size_type allocation_alignment{std::max({"
        << join(alignments, ", ") << "})};\n"
        << "static_assert(allocation_alignment <= std::numeric_limits<uint32>::max());\n\n";
    std::string previous{"0"};
    for (auto const& leaf : layout.leaves) {
        auto const id{fixed_leaf_argument(leaf)};
        if (id.find("__") != std::string::npos || id.back() == '_') {
            throw std::invalid_argument{
                "Single-allocation leaf would generate reserved identifiers: " + id};
        }
        if (!names.insert(id).second) {
            throw std::invalid_argument{"Single-allocation flattened leaf name collision: " + id};
        }
        auto const& type{leaf.type.spelling};
        dependencies.insert(
            dependencies.end(), leaf.type.dependencies.begin(), leaf.type.dependencies.end());
        out << "inline static constexpr byte_size_type " << id
            << "_block_offset{ml::single_allocation_experiment::layout_align(" << previous << ", "
            << type_ids.at(type) << "_alignment)};\n"
            << "static_assert(sizeof(" << type << ") <= (max_allocation_size - " << id
            << "_block_offset) / capacity_granularity);\n"
            << "inline static constexpr byte_size_type " << id << "_block_end{" << id
            << "_block_offset + capacity_granularity * sizeof(" << type << ")};\n\n";
        previous = id + "_block_end";
    }
    out << "inline static constexpr byte_size_type "
           "block_bytes{ml::single_allocation_experiment::layout_align("
        << previous << ", allocation_alignment)};\n"
        << "inline static constexpr size_type "
           "max_capacity{ml::single_allocation_experiment::maximum_capacity(block_bytes)};\n"
        << "static_assert(max_capacity >= capacity_granularity);\n\n"
        << "/* **************************************** */\n// Lifetime\n/* "
           "**************************************** */\n"
        << storage_name << "() noexcept = default;\n"
        << "~" << storage_name << "() { FMemory::Free(data_); }\n"
        << storage_name << "(" << storage_name << " const&) = delete;\n"
        << "auto operator=(" << storage_name << " const&) -> " << storage_name << "& = delete;\n"
        << storage_name << "(" << storage_name << "&& other) noexcept\n"
        << "    : data_{std::exchange(other.data_, nullptr)}, num_{std::exchange(other.num_, 0)}, "
           "capacity_{std::exchange(other.capacity_, 0)} {}\n"
        << "auto operator=(" << storage_name << "&& other) noexcept -> " << storage_name << "& {\n"
        << "    if (this != &other) {\n        FMemory::Free(data_);\n        data_ = "
           "std::exchange(other.data_, nullptr);\n        num_ = std::exchange(other.num_, 0);\n   "
           "     capacity_ = std::exchange(other.capacity_, 0);\n    }\n    return *this;\n}\n\n"
        << "protected:\n"
        << "template <typename Byte> struct DataPointers {\n"
        << "    template <typename T> using Element = std::conditional_t<std::is_const_v<Byte>, T "
           "const, T>;\n";
    for (auto const& leaf : layout.leaves) {
        out << "    Element<" << leaf.type.spelling << ">* " << fixed_leaf_argument(leaf)
            << "{};\n";
    }
    out << "};\n"
        << "template <typename Self> auto get_data(this Self& self) noexcept {\n"
        << "    using Byte = std::conditional_t<std::is_const_v<Self>, std::byte const, "
           "std::byte>;\n"
        << "    if (self.data_ == nullptr) { return DataPointers<Byte>{}; }\n"
        << "    return make_data_unchecked(static_cast<Byte*>(self.data_), "
           "self.capacity_blocks());\n}\n"
        << "template <typename Self> auto get_data(this Self& self, size_type const offset) "
           "noexcept {\n"
        << "    using Byte = std::conditional_t<std::is_const_v<Self>, std::byte const, "
           "std::byte>;\n"
        << "    if (self.data_ == nullptr) { return DataPointers<Byte>{}; }\n"
        << "    return make_data_unchecked(static_cast<Byte*>(self.data_), self.capacity_blocks(), "
           "offset);\n}\n\n"
        << "private:\nfriend struct ml::single_allocation_experiment::StorageOperations;\n"
        << "/* **************************************** */\n// Column pointers\n/* "
           "**************************************** */\n"
        << "template <typename Byte> static auto make_data_unchecked(Byte* const data, "
           "byte_size_type const blocks) noexcept -> DataPointers<Byte> {\n"
        << "    using Pointers = DataPointers<Byte>;\n    return {\n";
    for (auto const& leaf : layout.leaves) {
        auto const id{fixed_leaf_argument(leaf)};
        out << "        std::launder(reinterpret_cast<typename Pointers::template Element<"
            << leaf.type.spelling << ">*>(data + blocks * " << id << "_block_offset)),\n";
    }
    out << "    };\n}\n"
        << "template <typename Byte> static auto make_data_unchecked(Byte* const data, "
           "byte_size_type const blocks, size_type const offset) noexcept -> DataPointers<Byte> {\n"
        << "    auto columns{make_data_unchecked(data, blocks)};\n";
    for (auto const& leaf : layout.leaves) {
        out << "    columns." << fixed_leaf_argument(leaf) << " += offset;\n";
    }
    out << "    return columns;\n}\n"
        << "auto capacity_blocks() const noexcept -> byte_size_type { return "
           "static_cast<byte_size_type>(capacity_ / capacity_granularity); }\n\n"
        << "/* **************************************** */\n// Typed mutations and growth\n/* "
           "**************************************** */\n"
        << "void default_construct_columns(size_type const first, size_type const count) {\n"
        << "    auto const columns{make_data_unchecked(data_, capacity_blocks(), first)};\n";
    for (auto const& leaf : layout.leaves) {
        out << "    DefaultConstructItems<" << leaf.type.spelling << ">(columns."
            << fixed_leaf_argument(leaf) << ", count);\n";
    }
    out << "}\n"
        << "void swap_remove_columns(size_type const index, size_type const source, size_type "
           "const move_count) {\n"
        << "    auto const columns{make_data_unchecked(data_, capacity_blocks())};\n"
        << "    auto const elements_to_move{static_cast<byte_size_type>(move_count)};\n";
    emit_copy_sizes(out, "elements_to_move");
    for (auto const& leaf : layout.leaves) {
        auto const id{fixed_leaf_argument(leaf)};
        out << "    FMemory::Memcpy(columns." << id << " + index, columns." << id << " + source, "
            << type_ids.at(leaf.type.spelling) << "_bytes);\n";
    }
    out << "}\n"
        << "void reallocate(size_type const new_capacity) {\n"
        << "    auto* const "
           "new_data{ml::single_allocation_experiment::allocate(ml::single_allocation_experiment::"
           "allocation_bytes(new_capacity, block_bytes), "
           "static_cast<uint32>(allocation_alignment))};\n"
        << "    if (num_ > 0) {\n"
        << "        auto const old_blocks{capacity_blocks()};\n"
        << "        auto const new_blocks{static_cast<byte_size_type>(new_capacity / "
           "capacity_granularity)};\n"
        << "        auto const source{make_data_unchecked(static_cast<std::byte const*>(data_), "
           "old_blocks)};\n"
        << "        auto const destination{make_data_unchecked(new_data, new_blocks)};\n"
        << "        auto const live_count{static_cast<byte_size_type>(num_)};\n";
    emit_copy_sizes(out, "live_count");
    for (auto const& leaf : layout.leaves) {
        auto const id{fixed_leaf_argument(leaf)};
        out << "        FMemory::Memcpy(destination." << id << ", source." << id << ", "
            << type_ids.at(leaf.type.spelling) << "_bytes);\n";
    }
    out << "    }\n    FMemory::Free(data_);\n    data_ = new_data;\n    capacity_ = "
           "new_capacity;\n}\n"
        << "std::byte* data_{};\nsize_type num_{};\nsize_type capacity_{};\n};\n\n"
        << "struct " << name << " : " << storage_name << " {\n"
        << name << "() noexcept = default;\n"
        << name << "(" << name << " const&) = delete;\n"
        << "auto operator=(" << name << " const&) -> " << name << "& = delete;\n"
        << name << "(" << name << "&&) noexcept = default;\n"
        << "auto operator=(" << name << "&&) noexcept -> " << name << "& = default;\n\n"
        << "/* **************************************** */\n// Views\n/* "
           "**************************************** */\n";
    out << "private:\n";
    for (bool const is_const : {false, true}) {
        auto const view{is_const ? "ConstView" : "View"};
        auto const byte{is_const ? "std::byte const" : "std::byte"};
        out << "static auto make_view(DataPointers<" << byte
            << "> const& columns, size_type const count) -> " << view << " {\n"
            << "    return " << single_allocation_view_expression(schema, schemas, {}, is_const)
            << ";\n}\n";
    }
    out << "public:\n";
    for (bool const is_const : {false, true}) {
        auto const qualifier{is_const ? " const" : ""};
        auto const view{is_const ? "ConstView" : "View"};
        out << "auto get_view()" << qualifier << " -> " << view << " {\n"
            << "    auto const count{num()};\n    auto const columns{get_data()};\n"
            << "    return make_view(columns, count);\n}\n"
            << "auto get_view(size_type const offset, size_type const count)" << qualifier << " -> "
            << view << " {\n"
            << "    ml::single_allocation_experiment::require(offset >= 0 && offset <= num() && "
               "count >= 0 && count <= num() - offset);\n"
            << "    auto const columns{get_data(offset)};\n"
            << "    return make_view(columns, count);\n}\n"
            << "auto slice(size_type const offset, size_type const count)" << qualifier << " -> "
            << view << " { return get_view(offset, count); }\n"
            << "auto left(size_type const count)" << qualifier << " -> " << view
            << " { return get_view(0, count); }\n"
            << "auto right(size_type const count)" << qualifier << " -> " << view
            << " { ml::single_allocation_experiment::require(count >= 0 && count <= num()); return "
               "get_view(num() - count, count); }\n";
    }
    out << "auto get_const_view() const -> ConstView { return get_view(); }\n"
        << "auto get_const_view(size_type const offset, size_type const count) const -> ConstView "
           "{ return get_view(offset, count); }\n};";
    return raw(out.str(), std::move(dependencies));
}

} // namespace codegen::detail
