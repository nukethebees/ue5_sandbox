#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <lispb/indexed_column.h>
#include <lispb/schema/indices.h>
#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <limits>
#include <memory_resource>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>

namespace lispb::codegen {
namespace {
using MemberColumn = IndexedColumn<int, OneBasedIndex<RecordMemberId>>;
using TypeColumn = IndexedColumn<int, ZeroBasedIndex<schema::TypeId, UINT32_MAX>>;

template <typename Column, typename Id>
concept CanIndex = requires(Column& column, Id id) { column[id]; };

static_assert(CanIndex<MemberColumn, RecordMemberId>);
static_assert(CanIndex<MemberColumn, std::size_t>);
static_assert(!CanIndex<MemberColumn, ParameterId>);
static_assert(!CanIndex<MemberColumn, DeclarationHandle>);
static_assert(!CanIndex<MemberColumn, schema::RecordMemberId>);
static_assert(std::ranges::contiguous_range<MemberColumn>);
static_assert(sizeof(MemberColumn) == sizeof(std::vector<int>));
static_assert(alignof(MemberColumn) == alignof(std::vector<int>));
static_assert(std::is_same_v<decltype(std::declval<MemberColumn&>()[RecordMemberId{1}]), int&>);
static_assert(
    std::is_same_v<decltype(std::declval<MemberColumn const&>()[RecordMemberId{1}]), int const&>);
}

TEST(IndexedColumn, UsesExplicitIndexConventionsAndNumericAccess) {
    MemberColumn members{10, 20};
    TypeColumn types{30, 40};
    EXPECT_EQ(members[RecordMemberId{1}], 10);
    EXPECT_EQ(types[schema::TypeId{0}], 30);
    members[RecordMemberId{2}] = 50;
    EXPECT_EQ(members[1], 50);
    EXPECT_EQ(std::as_const(members)[RecordMemberId{2}], 50);
    EXPECT_EQ(std::as_const(types)[schema::TypeId{1}], 40);
}

TEST(IndexedColumn, PreservesVectorOperationsAndContiguousStorage) {
    MemberColumn column;
    column.reserve(8);
    auto const capacity{column.capacity()};
    column.assign({3, 1});
    column.push_back(2);
    column.emplace_back(4);
    std::ranges::sort(column);
    std::span<int> view{column};
    EXPECT_EQ(view.data(), column.data());
    EXPECT_EQ(&column[RecordMemberId{2}], view.data() + 1);
    std::erase(column, 2);
    column.insert(column.begin(), 0);
    column.resize(3);
    EXPECT_TRUE(std::ranges::equal(column, std::array{0, 1, 3}));
    EXPECT_EQ(column.capacity(), capacity);
    column.clear();
    EXPECT_TRUE(column.empty());
    EXPECT_EQ(column.capacity(), capacity);
}

TEST(IndexedColumn, PreservesCopyMoveAssignmentAndVectorInteroperability) {
    MemberColumn original{1, 2};
    auto copy{original};
    copy[RecordMemberId{1}] = 9;
    EXPECT_EQ(original[RecordMemberId{1}], 1);
    auto const* allocation{copy.data()};
    MemberColumn moved{std::move(copy)};
    EXPECT_EQ(moved.data(), allocation);
    copy = original;
    original = std::move(moved);
    EXPECT_EQ(original.data(), allocation);
    EXPECT_EQ(copy[RecordMemberId{1}], 1);
    original = std::vector{4, 5};
    original = {6, 7};
    std::vector<int>& base{original};
    base.push_back(8);
    EXPECT_TRUE(std::ranges::equal(original, std::array{6, 7, 8}));
}

TEST(IndexedColumn, SupportsProxyReferencesAndCustomAllocators) {
    IndexedColumn<bool, OneBasedIndex<RecordMemberId>> flags{false, true};
    flags[RecordMemberId{1}] = true;
    EXPECT_TRUE(flags[0]);
    EXPECT_TRUE(std::as_const(flags)[RecordMemberId{2}]);

    std::array<std::byte, 1024> buffer{};
    std::pmr::monotonic_buffer_resource memory{
        buffer.data(), buffer.size(), std::pmr::null_memory_resource()};
    IndexedColumn<int, OneBasedIndex<RecordMemberId>, std::pmr::polymorphic_allocator<int>> values{
        &memory};
    values.push_back(42);
    EXPECT_EQ(values[RecordMemberId{1}], 42);
    EXPECT_EQ(values.get_allocator().resource(), &memory);
}

TEST(IndexedColumn, TaggedStoresRetainAppendCloneAndClearSemantics) {
    RecordSchemas records;
    auto const record{records.append()};
    records.name[record] = StringId{11};
    records.s7_positional[record] = true;
    auto const clone{records.clone(record)};
    records.name[clone] = StringId{12};
    EXPECT_EQ(records.name[record], StringId{11});
    EXPECT_EQ(records.name[clone], StringId{12});
    EXPECT_TRUE(records.s7_positional[clone]);
    EXPECT_EQ(records.name[records.index(clone)], records.name[clone]);

    NormalModuleSchemas modules;
    auto const module{modules.append()};
    modules.s7_constructors[module] = true;
    EXPECT_TRUE(modules.s7_constructors[modules.clone(module)]);
    PackedFieldSchemas fields;
    auto const field{fields.append()};
    fields.bits[field] = 5;
    EXPECT_EQ(fields.bits[fields.clone(field)], 5);
    records.clear();
    EXPECT_TRUE(records.name.empty());
    EXPECT_FALSE(records.s7_positional[records.append()]);
}

TEST(IndexedColumn, RewritingClonesBeforeUpdatingAndPreservesHistoricalRows) {
    Strings strings;
    Declarations declarations{strings};
    auto& records{declarations.records};
    auto& members{declarations.record_members};
    auto const member{members.append()};
    members.type[member] = TypeRefId{1};
    auto const original{records.append()};
    records.members[original] = declarations.range<RecordMemberId>({member});
    auto const updated{rewrite_type_references(
        declarations, original, [](auto const&, TypeRefId, auto) { return TypeRefId{2}; })};
    EXPECT_NE(updated, original);
    EXPECT_EQ(members.type[member], TypeRefId{1});
    EXPECT_EQ(declarations.items(records.members[original]).front(), member);
    auto const updated_member{declarations.items(records.members[updated]).front()};
    EXPECT_NE(updated_member, member);
    EXPECT_EQ(members.type[updated_member], TypeRefId{2});
    auto const record_count{records.name.size()};
    auto const member_count{members.name.size()};
    EXPECT_EQ(rewrite_type_references(
                  declarations, updated, [](auto const&, TypeRefId id, auto) { return id; }),
              updated);
    EXPECT_EQ(records.name.size(), record_count);
    EXPECT_EQ(members.name.size(), member_count);
}

#ifndef NDEBUG
TEST(IndexedColumnDeathTest, RejectsInvalidHandlesWrongKindsAndMissingRows) {
    MemberColumn members{1};
    TypeColumn types{2};
    EXPECT_DEATH(static_cast<void>(members[RecordMemberId{}]), "");
    EXPECT_DEATH(static_cast<void>(members[RecordMemberId{2}]), "");
    EXPECT_DEATH(static_cast<void>(types[schema::TypeId{}]), "");
    RecordSchemas records;
    auto const record{records.append()};
    EXPECT_DEATH(static_cast<void>(records.name[DeclarationHandle{}]), "");
    EXPECT_DEATH(static_cast<void>(records.name[DeclarationHandle{
                     (static_cast<std::uint32_t>(DeclarationKind::enumeration) << 24) | 1}]),
                 "");
    EXPECT_DEATH(static_cast<void>(records.name[DeclarationHandle{record.value + 1}]), "");
    NormalModuleSchemas modules;
    static_cast<void>(modules.append());
    EXPECT_DEATH(static_cast<void>(modules.settings[ModuleHandle{
                     (static_cast<std::uint32_t>(ModuleKind::settings) << 24) | 1}]),
                 "");
    PackedFieldSchemas fields;
    static_cast<void>(fields.append());
    EXPECT_DEATH(static_cast<void>(fields.bits[PackedSegmentId{
                     (static_cast<std::uint32_t>(PackedSegmentKind::reserved) << 24) | 1}]),
                 "");
}
#endif
}
