#include "fixed_soa_internal.h"
#include "lowering_utils.h"

#include <set>
#include <sstream>
#include <stdexcept>

namespace codegen::detail {

static auto compact_columns_expression(SoaSchema const& schema,
                                       std::map<std::string, SoaSchema const*> const& schemas,
                                       std::map<std::string, CppType> const& leaves,
                                       std::vector<std::string> const& prefix,
                                       std::string const& layout,
                                       bool native) -> std::string {
    auto const mutable_view{schema.view_name.value_or(schema.name + "View")};
    auto const const_view{schema.const_view_name.value_or(schema.name + "ConstView")};
    std::vector<std::string> values;
    for (auto const& member : schema.members) {
        auto path{prefix};
        path.push_back(member.name);
        if (member.kind == SoaMemberKind::nested) {
            values.push_back(compact_columns_expression(
                *schemas.at(*member.nested_schema), schemas, leaves, path, layout, native));
        } else {
            auto const id{join(path, "_")};
            values.push_back("{this->template column_data_unchecked<" + leaves.at(id).spelling +
                             ">(" + layout + "::" + id + "_block_offset, blocks), " +
                             (native ? "static_cast<std::size_t>(this->count_)" : "this->count_") +
                             "}");
        }
    }
    return "std::conditional_t<Const, " + const_view + ", " + mutable_view + ">{" +
           join(values, ", ") + "}";
}

static void emit_compact_view(std::ostringstream& out,
                              SoaSchema const& schema,
                              std::map<std::string, SoaSchema const*> const& schemas,
                              std::map<std::string, CppType> const& leaves,
                              std::vector<std::string> const& prefix,
                              std::string const& root,
                              std::string const& layout,
                              std::string const& runtime,
                              bool native) {
    auto const name{root + (prefix.empty() ? "" : "_" + join(prefix, "_"))};
    for (auto const& member : schema.members) {
        if (member.kind == SoaMemberKind::nested) {
            auto path{prefix};
            path.push_back(member.name);
            emit_compact_view(out,
                              *schemas.at(*member.nested_schema),
                              schemas,
                              leaves,
                              path,
                              root,
                              layout,
                              runtime,
                              native);
        }
    }
    out << "template <bool Const> struct " << name << " : " << runtime
        << "CompactViewState<Const> {\n"
        << "using Base = " << runtime
        << "CompactViewState<Const>;\nusing size_type = typename Base::size_type;\nusing "
           "Base::Base;\n"
        << "using View = " << name << "<false>;\nusing ConstView = " << name << "<true>;\n"
        << name << "() = default;\n"
        << name << "(" << name << " const&) = default;\n"
        << "auto operator=(" << name << " const&) -> " << name << "& = default;\n"
        << name << "(" << name << "<false> const& other) requires Const : Base{other} {}\n"
        << "auto get_const_view() const -> " << name << "<true> { return *this; }\n"
        << "auto get_const_view(size_type offset, size_type count) const -> " << name
        << "<true> { return this->slice(offset, count); }\n";
    for (auto const& member : schema.members) {
        auto path{prefix};
        path.push_back(member.name);
        auto const id{join(path, "_")};
        if (member.kind == SoaMemberKind::nested) {
            out << "auto " << member.name << "() const { return " << root << "_" << id
                << "<Const>{this->state_, this->offset_, this->count_}; }\n";
        } else {
            auto const type{leaves.at(id).spelling};
            out << "auto " << member.name << "() const { return "
                << (native ? "std::span" : "TArrayView") << "<typename Base::template Element<"
                << type << ">>{this->template column_data<" << type << ">(" << layout << "::" << id
                << "_block_offset), "
                << (native ? "static_cast<std::size_t>(this->count_)" : "this->count_") << "}; }\n";
        }
    }
    auto const array_view{"std::conditional_t<Const, " +
                          schema.const_view_name.value_or(schema.name + "ConstView") + ", " +
                          schema.view_name.value_or(schema.name + "View") + ">"};
    out << "auto columns() const -> " << array_view << " {\nthis->validate();\n"
        << "if (!this->state_ || !this->state_->data_) { return {}; }\n"
        << "auto const blocks{this->capacity_blocks()};\nreturn "
        << compact_columns_expression(schema, schemas, leaves, prefix, layout, native) << ";\n}\n"
        << (native ? "template <typename Func> void each_column(Func&& func) const { "
                     "columns().each_column(std::forward<Func>(func)); }\n"
                   : "template <typename Func> auto apply_arrays(Func&& func) const -> "
                     "decltype(auto) { auto arrays{columns()}; return "
                     "arrays.apply_arrays(std::forward<Func>(func)); }\n")
        << "};\nstatic_assert(sizeof(" << name << "<false>) == 16 && sizeof(" << name
        << "<true>) == 16);\n"
        << "static_assert(std::is_trivially_copyable_v<" << name
        << "<false>> && std::is_trivially_copyable_v<" << name << "<true>>);\n";
}

auto lower_single_allocation_node(SoaSchema const& schema,
                                  std::map<std::string, SoaSchema const*> const& schemas,
                                  std::map<std::string, CppType> const& types,
                                  bool const native) -> Node {
    auto const* runtime{native ? "ml::native_soa::" : "ml::soa_storage::"};
    std::string free_data{native ? "ml::native_soa::free(data_, allocation_alignment)"
                                 : "ml::soa_storage::MimallocStorageAllocator::free(data_)"};
    auto const* copy{native ? "std::memcpy" : "FMemory::Memcpy"};
    auto const layout{build_soa_layout(schema, schemas, types, false)};
    auto const& name{*schema.single_allocation};
    auto const storage_name{name + "Storage"};
    auto const layout_name{schema.name + "SingleLayout"};
    auto const compact_view{schema.name + "SingleView"};
    std::vector<TypeDependency> dependencies{
        {"single_allocation_storage",
         native ? "native_soa/storage.h" : "SandboxCore/single_allocation_storage.h",
         {}}};
    std::string allocate{std::string{runtime} +
                         (native ? "allocate" : "MimallocStorageAllocator::allocate")};
    if (schema.single_allocation_allocator) {
        auto const allocator{resolve_type(*schema.single_allocation_allocator, types)};
        dependencies.insert(
            dependencies.end(), allocator.dependencies.begin(), allocator.dependencies.end());
        allocate = allocator.spelling + "::allocate";
        free_data = allocator.spelling + "::free(data_)";
    }
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
    std::ostringstream assertions;
    std::ostringstream out;
    std::ostringstream layout_output;
    out << "struct " << layout_name << " {\n"
        << "using size_type = " << (native ? "std::int32_t" : "int32")
        << ";\nusing byte_size_type = " << (native ? "std::size_t" : "SIZE_T") << ";\n\n"
        << "inline static constexpr byte_size_type "
           "max_allocation_size{std::numeric_limits<byte_size_type>::max()};\n"
        << "inline static constexpr size_type capacity_granularity{64};\n\n";
    std::vector<std::string> alignments;
    for (auto const* leaf : unique_types) {
        auto const id{fixed_leaf_argument(*leaf)};
        auto const& type{leaf->type.spelling};
        assertions
            << "static_assert(" << runtime << "supported_leaf<" << type
            << ">, \"Single-allocation leaf " << join(leaf->path, ".")
            << " requires a non-cv, trivially copyable/copy-constructible/destructible, nothrow "
               "default-constructible object type.\");\n";
        out << "inline static constexpr byte_size_type " << id << "_alignment{alignof(" << type
            << ") > 64 ? alignof(" << type << ") : 64};\n\n";
        alignments.push_back(id + "_alignment");
    }
    out << "inline static constexpr byte_size_type allocation_alignment{std::max({"
        << join(alignments, ", ") << "})};\n\n";
    assertions << "\nstatic_assert(allocation_alignment <= std::numeric_limits<"
               << (native ? "std::uint32_t" : "uint32") << ">::max());\n";
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
        out << "inline static constexpr byte_size_type " << id << "_block_offset{" << runtime
            << "layout_align(" << previous << ", " << type_ids.at(type) << "_alignment)};\n"
            << "inline static constexpr byte_size_type " << id << "_block_end{" << id
            << "_block_offset + capacity_granularity * sizeof(" << type << ")};\n\n";
        assertions << "static_assert(sizeof(" << type << ") <= (max_allocation_size - " << id
                   << "_block_offset) / capacity_granularity);\n";
        previous = id + "_block_end";
    }
    out << "inline static constexpr byte_size_type "
           "block_bytes{"
        << runtime << "layout_align(" << previous << ", allocation_alignment)};\n"
        << "inline static constexpr size_type "
           "max_capacity{"
        << runtime << "maximum_capacity(block_bytes)};\n"
        << "\nprivate:\ninline static constexpr auto validate_layout = []() consteval -> bool {\n"
        << assertions.str()
        << "static_assert(max_capacity >= capacity_granularity);\nreturn true;\n};\n"
        << "static_assert(validate_layout());\n};\n\n";
    layout_output << out.str();
    out.str({});
    if (!schema.single_allocation_allocator) {
        out << "template <bool Const> struct " << compact_view << ";\n" << layout_output.str();
    }
    out << "struct " << storage_name << " : " << layout_name << ", protected " << runtime
        << "StorageState, " << runtime << "StorageOperations {\n"
        << "using View = " << compact_view << "<false>;\n"
        << "using ConstView = " << compact_view << "<true>;\n"
        << "/* **************************************** */\n// Lifetime\n/* "
           "**************************************** */\n"
        << storage_name << "() noexcept = default;\n"
        << "~" << storage_name << "() { " << free_data << "; }\n"
        << storage_name << "(" << storage_name << " const&) = delete;\n"
        << "auto operator=(" << storage_name << " const&) -> " << storage_name << "& = delete;\n"
        << storage_name << "(" << storage_name << "&& other) noexcept\n"
        << "    : StorageState{std::exchange(other.data_, nullptr), std::exchange(other.num_, 0), "
           "std::exchange(other.capacity_, 0)} {}\n"
        << "auto operator=(" << storage_name << "&& other) noexcept -> " << storage_name << "& {\n"
        << "    if (this != &other) {\n        " << free_data
        << ";\n        data_ = "
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
        << "private:\nfriend struct " << runtime << "StorageOperations;\n"
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
        out << "    "
            << (native ? "std::uninitialized_value_construct_n<" : "DefaultConstructItems<")
            << leaf.type.spelling << (native ? "*>" : ">") << "(columns."
            << fixed_leaf_argument(leaf) << ", count);\n";
    }
    out << "}\n"
        << "void swap_remove_columns(size_type const index, size_type const source, size_type "
           "const move_count) {\n"
        << "    copy_columns(get_data(), index, source, move_count);\n}\n"
        << "static void copy_columns(DataPointers<std::byte> const& columns, size_type index, "
           "size_type source, size_type move_count) {\n"
        << "    auto const elements_to_move{static_cast<byte_size_type>(move_count)};\n";
    emit_copy_sizes(out, "elements_to_move");
    for (auto const& leaf : layout.leaves) {
        auto const id{fixed_leaf_argument(leaf)};
        out << "    " << copy << "(columns." << id << " + index, columns." << id << " + source, "
            << type_ids.at(leaf.type.spelling) << "_bytes);\n";
    }
    out << "}\n"
        << "void swap_remove_indices(std::span<size_type const> indices) {\n"
        << "auto const columns{get_data()};\n"
        << "ml::soa_storage_detail::for_each_removal_run(num_, indices, " << runtime
        << "require, [&](size_type index, size_type source, size_type count) { "
           "copy_columns(columns, index, source, count); });\n}\n"
        << "template <typename Columns> void append_columns(Columns const& source, size_type "
           "first, size_type count) {\n"
        << "auto const destination{get_data(first)};\nauto const "
           "elements_to_copy{static_cast<byte_size_type>(count)};\n";
    emit_copy_sizes(out, "elements_to_copy");
    for (auto const& leaf : layout.leaves) {
        out << copy << "(destination." << fixed_leaf_argument(leaf) << ", source."
            << join(leaf.path, ".") << (native ? ".data()" : ".GetData()") << ", "
            << type_ids.at(leaf.type.spelling) << "_bytes);\n";
    }
    out << "}\n"
        << "void reallocate(size_type const new_capacity) {\n"
        << "    auto* const "
           "new_data{"
        << allocate << "(" << runtime
        << ""
           "allocation_bytes(new_capacity, block_bytes), "
           "static_cast<"
        << (native ? "std::uint32_t" : "uint32") << ">(allocation_alignment))};\n"
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
        out << "        " << copy << "(destination." << id << ", source." << id << ", "
            << type_ids.at(leaf.type.spelling) << "_bytes);\n";
    }
    out << "    }\n    " << free_data
        << ";\n    data_ = new_data;\n    capacity_ = "
           "new_capacity;\n}\n"
        << "};\n\n";
    if (!schema.single_allocation_allocator) {
        std::map<std::string, CppType> leaf_types;
        for (auto const& leaf : layout.leaves) {
            leaf_types.emplace(fixed_leaf_argument(leaf), leaf.type);
        }
        emit_compact_view(
            out, schema, schemas, leaf_types, {}, compact_view, layout_name, runtime, native);
    }
    out << "struct " << name << " : " << storage_name << " {\n"
        << name << "() noexcept = default;\n"
        << name << "(" << name << " const&) = delete;\n"
        << "auto operator=(" << name << " const&) -> " << name << "& = delete;\n"
        << name << "(" << name << "&&) noexcept = default;\n"
        << "auto operator=(" << name << "&&) noexcept -> " << name << "& = default;\n";
    for (bool const is_const : {false, true}) {
        auto const qualifier{is_const ? " const" : ""};
        auto const view{is_const ? "ConstView" : "View"};
        out << "auto get_view()" << qualifier << " -> " << view << " { return {this, 0, num()}; }\n"
            << "auto get_view(size_type offset, size_type count)" << qualifier << " -> " << view
            << " { return {this, offset, count}; }\n"
            << "auto slice(size_type offset, size_type count)" << qualifier << " -> " << view
            << " { return get_view(offset, count); }\n"
            << "auto left(size_type count)" << qualifier << " -> " << view
            << " { return get_view().left(count); }\n"
            << "auto right(size_type count)" << qualifier << " -> " << view
            << " { return get_view().right(count); }\n";
    }
    out << "auto get_const_view() const -> ConstView { return get_view(); }\n"
        << "auto get_const_view(size_type offset, size_type count) const -> ConstView { return "
           "get_view(offset, count); }\n};";
    return raw(out.str(), std::move(dependencies));
}

} // namespace codegen::detail
