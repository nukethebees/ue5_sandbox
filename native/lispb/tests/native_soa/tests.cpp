#include "../pmr_owner_tests.h"

#include <gtest/gtest.h>
#include <native_soa/types.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ioj::native_soa_tests {
using namespace native_soa_fixture;

TEST(NativeSoa, CountColumnDoesNotCollideWithViewRowCount) {
    Counts owner;
    owner.add(17U);
    owner.add(29U);
    auto const view{owner.get_view()};
    auto const resolved{view.resolve()};
    EXPECT_EQ(resolved.num(), 2U);
    EXPECT_EQ(resolved.count()[0], 17U);

    counts::ResolvedView pointers{owner.count().data(), owner.num()};
    counts::ResolvedView spans{owner.count()};
    counts::ResolvedConstView const_view{resolved};
    EXPECT_EQ(pointers.count().data(), spans.count().data());
    EXPECT_EQ(const_view.slice(1, 1).count()[0], 29U);
    EXPECT_EQ(view.slice(1, 1).resolve().num(), 1U);
    spans.set(0, 41U);
    EXPECT_EQ(owner.count()[0], 41U);
}

template <typename T>
concept CanSetCount = requires(T&& value) { std::forward<T>(value).set(0U, 1U); };

TEST(NativeSoa, FlatRowsPreserveTypedMemberFunctionsAndReceiverQualifiers) {
    static_assert(CanSetCount<Counts&> && CanSetCount<Counts>);
    static_assert(!CanSetCount<Counts const&>);
    static_assert(CanSetCount<counts::View const&>);
    static_assert(CanSetCount<counts::ResolvedView const&>);
    static_assert(!CanSetCount<counts::ConstView>);
    static_assert(!CanSetCount<counts::ResolvedConstView>);

    auto const add{static_cast<std::uint32_t (Counts::*)(std::uint32_t)>(&Counts::add)};
    auto const set{static_cast<void (Counts::*)(std::uint32_t, std::uint32_t)>(&Counts::set)};
    auto const set_view{static_cast<void (counts::View::*)(std::uint32_t, std::uint32_t) const>(
        &counts::View::set)};
    Counts owner;
    EXPECT_EQ((owner.*add)(3U), 0U);
    (owner.*set)(0U, 5U);
    auto const view{owner.get_view()};
    (view.*set_view)(0U, 7U);
    EXPECT_EQ(owner.count()[0], 7U);
    view.resolve().set(0, 11U);
    EXPECT_EQ(owner.count()[0], 11U);
    EXPECT_DEATH(view.set(1, 0), "");
    EXPECT_DEATH(view.resolve().set(1, 0), "");

    PairRows pairs;
    pairs.add({13.0f, 17.0f}, nullptr);
    EXPECT_EQ(pairs.pairs()[0].x, 13.0f);
    EXPECT_EQ(pairs.pointers()[0], nullptr);
}

TEST(NativeSoa, FlatInsertionFailurePreservesStorageAndLiveRows) {
    pmr_test_support::AllocationState state;
    pmr_test_support::CountingResource resource{state};
    Counts owner{resource};
    owner.add(19U);
    owner.set_num(owner.capacity());
    auto const capacity{owner.capacity()};
    auto const* data{owner.count().data()};
    auto const allocations{state.allocations};

    state.reject_allocation = true;
    EXPECT_THROW(owner.add(owner.count()[0]), std::bad_alloc);
    EXPECT_EQ(owner.num(), capacity);
    EXPECT_EQ(owner.capacity(), capacity);
    EXPECT_EQ(owner.count().data(), data);
    EXPECT_EQ(owner.count()[0], 19U);
    EXPECT_EQ(state.allocations, allocations);

    state.reject_allocation = false;
    EXPECT_EQ(owner.add(owner.count()[0]), capacity);
    EXPECT_EQ(owner.count()[capacity], 19U);
    EXPECT_EQ(state.allocations, allocations + 1);
    owner.add(23U);
    EXPECT_EQ(state.allocations, allocations + 1);
}

TEST(NativeSoa, PmrResourcesAndMoves) {
    pmr_test_support::verify_pmr_owner<SingleRows>(
        [](SingleRows& owner) -> auto& { return owner.get_view().values()[0]; });
}

TEST(NativeSoa, ResourceAlignmentAndFailurePreserveAllColumns) {
    pmr_test_support::AllocationState state;
    pmr_test_support::CountingResource resource{state};
    {
        SingleAlignmentRows owner{resource};
        owner.set_num(1);
        owner.get_view().aligned256()[0].value = 123;
        auto const old_capacity{owner.capacity()};
        auto* const old_data{owner.get_view().aligned256().data()};
        state.reject_allocation = true;
        EXPECT_THROW(owner.reserve(old_capacity + 1), std::bad_alloc);
        EXPECT_EQ(owner.capacity(), old_capacity);
        EXPECT_EQ(owner.num(), 1);
        EXPECT_EQ(owner.get_view().aligned256().data(), old_data);
        EXPECT_EQ(owner.get_view().aligned256()[0].value, 123);
        state.reject_allocation = false;
        owner.reserve(old_capacity + 1);
        EXPECT_EQ(owner.get_view().aligned256()[0].value, 123);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(owner.get_view().aligned256().data()) % 256, 0);
        EXPECT_EQ(state.last_alignment, SingleAlignmentRows::allocation_alignment);
        EXPECT_EQ(state.last_bytes, owner.allocated_bytes());
    }
    EXPECT_TRUE(state.live.empty());
    EXPECT_EQ(state.allocations, state.frees);
}

TEST(NativeSoa, ResourceMovesPreserveLogicalApiOwners) {
    pmr_test_support::AllocationState first_state;
    pmr_test_support::AllocationState second_state;
    pmr_test_support::CountingResource first{first_state};
    pmr_test_support::CountingResource second{second_state};
    {
        ApiOwner source{first};
        source.set_num(1);
        source.get_view().values()[0] = 19;
        ApiOwner destination{second};
        destination = std::move(source);
        EXPECT_EQ(destination.first_value(), 19);
        EXPECT_EQ(destination.get_memory_resource(), &second);
        // NOLINTNEXTLINE(bugprone-use-after-move): Verify the defined moved-from state.
        EXPECT_EQ(source.num(), 0);
    }
    EXPECT_TRUE(first_state.live.empty());
    EXPECT_TRUE(second_state.live.empty());
}

static_assert(!native_soa::supported_leaf<std::string>);
static_assert(!native_soa::supported_leaf<float const>);
static_assert(!native_soa::supported_leaf<float volatile>);
static_assert(!native_soa::supported_leaf<float&>);
static_assert(!native_soa::supported_leaf<float[3]>);
static_assert(!native_soa::supported_leaf<void>);
static_assert(!std::is_copy_constructible_v<SingleRows>);
template <typename T>
concept CanReset = requires(T& value) { value.reset(); };
template <typename T>
concept CanAddUninitialised = requires(T& value) { value.add_uninitialised(1); };
template <typename T>
concept CanAssignFirst = requires(T value) { value.assign_first(1.0f); };
template <typename T>
concept CanReserve = requires(T& value) { value.reserve(1); };
template <typename T>
concept CanSetNum = requires(T& value) { value.set_num(1); };
template <typename T, typename Source>
concept CanCopy =
    requires(T& value, Source const& source) { value.copy_elements(0, source, 0, 1); };
template <typename T, typename Source>
concept CanAppend = requires(T& value, Source const& source) { value.append_from(source); };
template <typename T>
concept CanReallocate = requires(T& value) { value.reallocate(1); };
template <typename T>
concept CanReleaseStorage = requires(T& value) { value.release_storage(value); };
template <typename T>
concept CanBorrow = requires(T&& value) { std::forward<T>(value).get_view(); };
struct MissingStorage : native_soa::StorageOperations {};

static_assert(CanReset<ApiOwner>);
static_assert(CanAddUninitialised<ApiOwner>);
static_assert(CanReserve<ApiOwner>);
static_assert(CanSetNum<ApiOwner>);
static_assert(!CanReset<ApiOwner const>);
static_assert(!CanAddUninitialised<ApiOwner const>);
static_assert(!CanReserve<ApiOwner const>);
static_assert(!CanSetNum<ApiOwner const>);
static_assert(!CanReset<MissingStorage>);
static_assert(!CanReserve<MissingStorage>);
static_assert(!CanSetNum<MissingStorage>);
static_assert(CanCopy<ApiOwner, api_owner::ConstView>);
static_assert(CanAppend<ApiOwner, api_owner::ConstView>);
static_assert(!CanCopy<ApiOwner const, api_owner::ConstView>);
static_assert(!CanAppend<ApiOwner const, api_owner::ConstView>);
static_assert(!CanCopy<ApiOwner, int>);
static_assert(!CanAppend<ApiOwner, int>);
static_assert(soa_storage_detail::SameSoaSchema<ApiOwner, api_owner::ConstView>);
static_assert(!soa_storage_detail::SameSoaSchema<ApiOwner, SingleRows>);
static_assert(!soa_storage_detail::SameSoaSchema<ApiOwner, MissingStorage>);
static_assert(CanAppend<ApiOwner, ApiOwner>);
static_assert(CanCopy<ApiOwner, ApiOwner>);
static_assert(!CanCopy<ApiOwner, SingleRows::ConstView>);
static_assert(!CanAppend<ApiOwner, SingleRows::ConstView>);
static_assert(!CanReallocate<ApiOwner>);
static_assert(!CanReleaseStorage<ApiOwner>);
static_assert(CanBorrow<ApiOwner&>);
static_assert(CanBorrow<ApiOwner const&>);
static_assert(!CanBorrow<ApiOwner>);
static_assert(!CanBorrow<ApiOwner const>);
static_assert(!CanBorrow<MissingStorage&>);
static_assert(CanAssignFirst<api_owner::View>);
static_assert(!CanAssignFirst<api_owner::ConstView>);
static_assert(soa_storage_detail::validate_compact_view<api_owner::View>());
static_assert(soa_storage_detail::validate_compact_view<api_owner::ConstView>());
static_assert(std::is_same_v<ApiOwner::Value, api_owner::ConstView::Value>);
static_assert(std::is_same_v<decltype(ApiReference::owner), ApiOwner*>);
static_assert(std::is_same_v<decltype(std::declval<SingleRows const&>().get_view().values()),
                             std::span<std::int32_t const>>);

TEST(NativeSoa, MutuallyReferencingPointersCompileInBothOrdersAndAcrossDeclarationKinds) {
    PointerA first;
    PointerB second;
    first.add(&second);
    second.add(&first);
    EXPECT_EQ(first.get_view().others()[0], &second);
    EXPECT_EQ(second.get_view().others()[0], &first);
    MixedRows rows;
    MixedRecord record{&rows};
    rows.add(&record);
    EXPECT_EQ(record.rows->get_view().records()[0], &record);
}

TEST(NativeSoa, LogicalApiSurvivesCompactOnlyStorageAndNestedLayouts) {
    static_assert(ApiOwner::schema_version() == 7);
    static_assert(ApiOwner::identity(9) == 9);
    static_assert(api_owner::ConstView{}.schema_version() == 7);
    static_assert(api_owner::View{}.mutable_version() == 8);
    ApiOwner owner{};
    owner.set_num(2);
    auto view{owner.get_view()};
    view.assign_first(7.0f);
    view.values()[1] = 9.0f;
    auto positions{view.view_positions()};
    static_assert(std::is_same_v<decltype(positions), api_coordinates::View>);
    positions.xs()[0] = 2.0f;
    positions.ys()[0] = 3.0f;
    positions.shift_x(4.0f);
    view.masks()[0].set(ApiField::Values);

    EXPECT_EQ(owner.first_value(), 7.0f);
    EXPECT_EQ(owner.total(), 16.0f);
    EXPECT_EQ(view.first_x(), 6.0f);
    EXPECT_EQ(owner.get_const_view().first_x(), 6.0f);
    EXPECT_EQ(positions.x_at(0), 6.0f);
    EXPECT_EQ(positions[0].y, 3.0f);
    EXPECT_TRUE(view.masks()[0].has(ApiField::Values));

    owner.reserve(owner.capacity() + 1);
    view = owner.get_view();
    positions = view.view_positions();
    EXPECT_EQ(view.first_x(), 6.0f);
    EXPECT_EQ(positions.x_at(0), 6.0f);
    EXPECT_EQ(owner.append_from(view.slice(0, 1)), 2);
    owner.copy_elements(1, owner.get_const_view(), 0, 2);
    EXPECT_EQ(owner.get_const_view().values()[2], 9.0f);
    EXPECT_EQ(owner.get_const_view().view_positions()[1].x, 6.0f);

    EquivalentOwner equivalent{};
    equivalent.set_num(1);
    equivalent.get_view().xs()[0] = 5.0f;
    equivalent.get_view().ys()[0] = 8.0f;
    EXPECT_EQ(equivalent[0].x, 5.0f);
    EXPECT_EQ(equivalent.get_const_view()[0].y, 8.0f);
}

TEST(NativeSoa, PartialAppendDefaultsUnspecifiedColumnsAcrossGrowthAndReuse) {
    ApiOwner owner;
    EXPECT_EQ(owner.append_value(17.0f), 0U);
    owner.set_num(owner.capacity());
    auto const row{owner.num()};
    EXPECT_EQ(owner.append_value(owner.values()[0]), row);
    EXPECT_EQ(owner.values()[row], 17.0f);
    EXPECT_FALSE(owner.masks()[row].has(ApiField::Values));
    EXPECT_EQ(owner.view_positions()[row].x, 0.0f);
    EXPECT_EQ(owner.view_positions()[row].y, 0.0f);

    owner.masks()[0].set(ApiField::Values);
    owner.view_positions().set(0, ApiPair{3.0f, 4.0f});
    owner.reset();
    EXPECT_EQ(owner.append_empty(), 0U);
    EXPECT_EQ(owner.num(), 1U);
    EXPECT_EQ(owner.values()[0], 0.0f);
    EXPECT_FALSE(owner.masks()[0].has(ApiField::Values));
    EXPECT_EQ(owner.view_positions()[0].x, 0.0f);
    EXPECT_EQ(owner.view_positions()[0].y, 0.0f);
}

TEST(NativeSoa, NestedEquivalentValuesSupportOwnerAndMutableViewRowOperations) {
    ApiOwner owner;
    EXPECT_EQ(owner.add(1.0f, ApiMask{}, ApiPair{2.0f, 3.0f}), 0U);
    EXPECT_EQ(owner.add(4.0f, ApiMask{}, 5.0f, 6.0f), 1U);
    EXPECT_EQ(owner.values()[0], 1.0f);
    EXPECT_EQ(owner.view_positions()[0].x, 2.0f);
    EXPECT_EQ(owner.view_positions()[0].y, 3.0f);
    EXPECT_EQ(owner.view_positions()[1].x, 5.0f);
    EXPECT_EQ(owner.view_positions()[1].y, 6.0f);

    ApiMask mask{};
    mask.set(ApiField::Values);
    owner.set(0, 7.0f, mask, ApiPair{8.0f, 9.0f});
    auto const view{owner.get_view()};
    view.set(1, 10.0f, mask, ApiPair{11.0f, 12.0f});
    EXPECT_EQ(view.values()[0], 7.0f);
    EXPECT_TRUE(view.masks()[0].has(ApiField::Values));
    EXPECT_EQ(view.view_positions()[0].x, 8.0f);
    EXPECT_EQ(view.view_positions()[0].y, 9.0f);
    EXPECT_EQ(view.values()[1], 10.0f);
    EXPECT_TRUE(view.masks()[1].has(ApiField::Values));
    EXPECT_EQ(view.view_positions()[1].x, 11.0f);
    EXPECT_EQ(view.view_positions()[1].y, 12.0f);

    auto const resolved{view.resolve()};
    resolved.set(1, 13.0f, ApiMask{}, ApiPair{14.0f, 15.0f});
    EXPECT_EQ(view.values()[1], 13.0f);
    EXPECT_FALSE(view.masks()[1].has(ApiField::Values));
    EXPECT_EQ(view.view_positions()[1].x, 14.0f);
    EXPECT_EQ(view.view_positions()[1].y, 15.0f);
}

TEST(NativeSoa, ResolvedViewsBorrowColumnsAndPreserveNestedLogicalApi) {
    static_assert(std::is_trivially_copyable_v<api_owner::ResolvedView>);
    static_assert(std::is_trivially_copyable_v<api_owner::ResolvedConstView>);
    static_assert(std::is_standard_layout_v<api_owner::ResolvedView>);
    static_assert(std::is_standard_layout_v<api_owner::ResolvedConstView>);
    static_assert(sizeof(api_owner::ResolvedView) == 5 * sizeof(void*));
    static_assert(sizeof(api_owner::ResolvedConstView) == sizeof(api_owner::ResolvedView));
    static_assert(std::is_convertible_v<api_owner::ResolvedView, api_owner::ResolvedConstView>);
    static_assert(!std::is_convertible_v<api_owner::ResolvedConstView, api_owner::ResolvedView>);
    pmr_test_support::AllocationState state;
    pmr_test_support::CountingResource resource{state};
    ApiOwner owner{resource};
    owner.set_num(3);
    auto const allocations{state.allocations};
    auto const compact{owner.get_view()};
    auto resolved{compact.resolve()};
    auto positions{resolved.view_positions()};
    static_assert(std::is_same_v<decltype(positions), api_coordinates::ResolvedView>);
    resolved.assign_first(7.0f);
    positions.xs()[1] = 4.0f;
    positions.ys()[1] = 9.0f;
    positions.shift_x(2.0f);

    EXPECT_EQ(owner.first_value(), 7.0f);
    EXPECT_EQ(resolved.values().data(), compact.values().data());
    EXPECT_EQ(positions.xs().data(), compact.view_positions().xs().data());
    EXPECT_EQ(resolved.slice(1, 1).first_x(), 6.0f);
    EXPECT_EQ(positions.slice(1, 1)[0].y, 9.0f);
    EXPECT_EQ(compact.view_positions().resolve().xs().data(), positions.xs().data());
    EXPECT_EQ(resolved.right(1).num(), 1U);
    EXPECT_EQ(state.allocations, allocations);

    api_owner::ResolvedConstView const immutable{resolved};
    static_assert(std::is_same_v<decltype(immutable.values()), std::span<float const>>);
    static_assert(
        std::is_same_v<decltype(immutable.view_positions().xs()), std::span<float const>>);
    static_assert(!CanAssignFirst<api_owner::ResolvedConstView>);
    EXPECT_EQ(immutable.values().data(), resolved.values().data());
    static_assert(
        std::is_same_v<decltype(immutable.get_const_view()), api_owner::ResolvedConstView>);
    static_assert(
        std::is_same_v<decltype(immutable.get_const_view(1, 1)), api_owner::ResolvedConstView>);
    EXPECT_EQ(immutable.get_const_view().values().data(), resolved.values().data());
    EXPECT_EQ(immutable.get_const_view(1, 1).view_positions()[0].x, 6.0f);
    EXPECT_EQ(resolved.get_const_view(1, 1).view_positions()[0].x, 6.0f);
    EXPECT_EQ(owner.get_const_view().resolve().values().data(), resolved.values().data());
}

TEST(NativeSoa, ResolvedViewsAcceptIndependentSpansAndServeAsAppendSources) {
    std::array values{3.0f, 5.0f};
    std::array<ApiMask, 2> masks{};
    std::array xs{7.0f, 11.0f};
    std::array ys{13.0f, 17.0f};
    api_owner::ResolvedView source{
        std::span{values}, std::span{masks}, std::span{xs}, std::span{ys}};
    EXPECT_EQ(source.view_positions().xs().data(), xs.data());
    EXPECT_EQ(source.view_positions().ys().data(), ys.data());
    source.view_positions().shift_x(1.0f);
    EXPECT_EQ(xs[1], 12.0f);
    ApiOwner owner;
    owner.append_from(source);
    EXPECT_EQ(owner.total(), 8.0f);
    EXPECT_EQ(owner.get_const_view().view_positions()[1].x, 12.0f);

    EXPECT_DEATH((api_owner::ResolvedView{
                     std::span{values}, std::span{masks}, std::span{xs}.first(1), std::span{ys}}),
                 "");
    EXPECT_DEATH((source.slice(1, 2)), "");
    EXPECT_DEATH((api_owner::ResolvedView{values.data(), masks.data(), xs.data(), nullptr, 2}), "");
    EXPECT_DEATH(ioj::soa_storage_detail::checked_row_count(
                     std::size_t{std::numeric_limits<std::uint32_t>::max()} + 1),
                 "");
}

TEST(NativeSoa, ScalarInsertionAndAssignmentPreserveNestedColumnsAcrossGrowth) {
    ApiOwner owner;
    EXPECT_EQ(owner.add(1.0f, ApiMask{}, 2.0f, 3.0f), 0U);
    EXPECT_EQ(owner.values()[0], 1.0f);
    EXPECT_EQ(owner.view_positions()[0].y, 3.0f);
    owner.set_num(owner.capacity());
    auto const first{owner.num()};
    EXPECT_EQ(owner.add(owner.values()[0],
                        owner.masks()[0],
                        owner.view_positions().xs()[0],
                        owner.view_positions().ys()[0]),
              first);
    EXPECT_EQ(owner.values()[first], 1.0f);
    EXPECT_EQ(owner.view_positions()[first].x, 2.0f);
    EXPECT_EQ(owner.view_positions()[first].y, 3.0f);

    auto rows{owner.get_view().resolve()};
    rows.set(first, 4.0f, ApiMask{}, 5.0f, 6.0f);
    EXPECT_EQ(owner.values()[first], 4.0f);
    EXPECT_EQ(owner.view_positions()[first].y, 6.0f);
    owner.get_view().set(first, 7.0f, ApiMask{}, 8.0f, 9.0f);
    EXPECT_EQ(owner.values()[first], 7.0f);
    EXPECT_EQ(owner.view_positions()[first].x, 8.0f);
    auto const positions{owner.view_positions()};
    positions.set(first, positions.ys()[first], positions.xs()[first]);
    EXPECT_EQ(positions.xs()[first], 9.0f);
    EXPECT_EQ(positions.ys()[first], 8.0f);
    static_assert(std::is_same_v<decltype(std::as_const(owner).values()), std::span<float const>>);
}

TEST(NativeSoa, OrdinaryColumnsMapEquivalentValuesForOwnersAndBothViews) {
    EquivalentOwner owner;
    EXPECT_EQ(owner.add(ApiPair{3.0f, 7.0f}), 0U);
    EXPECT_EQ(owner[0].x, 3.0f);
    EXPECT_EQ(owner[0].y, 7.0f);
    owner.get_view().set(0, ApiPair{11.0f, 13.0f});
    EXPECT_EQ(owner[0].y, 13.0f);
    owner.get_view().resolve().set(0, ApiPair{17.0f, 19.0f});
    EXPECT_EQ(owner[0].x, 17.0f);
    owner.set(0, ApiPair{23.0f, 29.0f});
    EXPECT_EQ(owner.xs()[0], 23.0f);
    EXPECT_EQ(owner.ys()[0], 29.0f);
    EXPECT_DEATH(owner[1], "");

    AccessorOwner mapped;
    mapped.add(Vector3f{31.0f, 37.0f, 41.0f});
    EXPECT_EQ(mapped.horizontal()[0], 31.0f);
    EXPECT_EQ(mapped.get_const_view().resolve()[0].y(), 37.0f);
    mapped.get_view().resolve().set(0, Vector3f{43.0f, 47.0f, 53.0f});
    EXPECT_EQ(mapped[0].z(), 53.0f);
}

TEST(NativeSoa, EmptyResolvedViewsCanBeSlicedAndConverted) {
    ApiOwner owner;
    auto const resolved{owner.get_view().resolve()};
    EXPECT_TRUE(resolved.is_empty());
    EXPECT_TRUE(resolved.slice(0, 0).is_empty());
    EXPECT_TRUE(resolved.get_const_view().view_positions().is_empty());
    EXPECT_EQ(resolved.values().data(), nullptr);
    EXPECT_EQ(resolved.view_positions().xs().data(), nullptr);
    EXPECT_EQ(api_owner::View{}.resolve().values().data(), nullptr);
    owner.reserve(2);
    EXPECT_EQ(owner.get_const_view().resolve().values().data(), owner.values().data());
}

TEST(NativeSoa, ResolvedSelfAppendPreservesAliasedColumnsAcrossGrowthAndAllocationFailure) {
    pmr_test_support::AllocationState state;
    pmr_test_support::CountingResource resource{state};
    ApiOwner owner{resource};
    owner.reserve(1);
    owner.set_num(owner.capacity());
    auto rows{owner.get_view().resolve()};
    rows.values()[0] = 3.0f;
    rows.values()[1] = 5.0f;
    rows.view_positions().xs()[0] = 7.0f;
    rows.view_positions().ys()[1] = 11.0f;
    rows.masks()[1].set(ApiField::Values);
    auto const source{rows.get_const_view(0, 2)};
    auto const first{owner.num()};
    auto const allocations{state.allocations};

    state.reject_allocation = true;
    EXPECT_THROW(owner.append_from(source), std::bad_alloc);
    EXPECT_EQ(owner.num(), first);
    EXPECT_EQ(owner.get_view().values().data(), source.values().data());
    EXPECT_EQ(source.values()[1], 5.0f);

    state.reject_allocation = false;
    EXPECT_EQ(owner.append_from(source), first);
    auto const appended{owner.get_const_view().slice(first, 2).resolve()};
    EXPECT_EQ(appended.values()[0], 3.0f);
    EXPECT_EQ(appended.values()[1], 5.0f);
    EXPECT_EQ(appended.view_positions()[0].x, 7.0f);
    EXPECT_EQ(appended.view_positions()[1].y, 11.0f);
    EXPECT_TRUE(appended.masks()[1].has(ApiField::Values));
    EXPECT_EQ(state.allocations, allocations + 1);
}

class IndexedSelfAppend : public testing::TestWithParam<bool> {};

TEST_P(IndexedSelfAppend, PreservesEveryColumnWithAndWithoutGrowth) {
    auto const grow{GetParam()};
    pmr_test_support::AllocationState state;
    pmr_test_support::CountingResource resource{state};
    SingleRows owner{resource};
    owner.reserve(1);
    owner.set_num(grow ? owner.capacity() : 4);
    auto const first{owner.num()};
    std::uint32_t column_index{};
    owner.get_view().each_column([&](auto column) {
        using Element = typename decltype(column)::value_type;
        for (std::uint32_t row{}; row < first; ++row) {
            column[row] = static_cast<Element>(column_index * 1000 + row);
        }
        ++column_index;
    });
    auto const source{owner.get_const_view(1, 3).resolve()};
    std::array<std::uint32_t, 4> const indices{2, 0, 2, 1};
    auto const allocations{state.allocations};
    EXPECT_EQ(owner.append_from(source, std::span<std::uint32_t const>{}), first);
    EXPECT_EQ(state.allocations, allocations);
    std::array<std::uint32_t, 1> const outside_source{3};
    EXPECT_DEATH(owner.append_from(source, outside_source), "");

    if (grow) {
        state.reject_allocation = true;
        EXPECT_THROW(owner.append_from(source, indices), std::bad_alloc);
        EXPECT_EQ(owner.num(), first);
        EXPECT_EQ(owner.values().data() + 1, source.values().data());
        state.reject_allocation = false;
    }

    EXPECT_EQ(owner.append_from(source, indices), first);
    EXPECT_EQ(owner.num(), first + indices.size());
    EXPECT_EQ(state.allocations, allocations + (grow ? 1 : 0));
    column_index = 0;
    owner.get_const_view().each_column([&](auto column) {
        using Element = typename decltype(column)::value_type;
        for (std::uint32_t row{}; row < first; ++row) {
            EXPECT_EQ(column[row], static_cast<Element>(column_index * 1000 + row));
        }
        auto const count{static_cast<std::uint32_t>(indices.size())};
        for (std::uint32_t row{}; row < count; ++row) {
            EXPECT_EQ(column[first + row],
                      static_cast<Element>(column_index * 1000 + indices[row] + 1));
        }
        ++column_index;
    });
    EXPECT_EQ(column_index, 7U);
}

INSTANTIATE_TEST_SUITE_P(NativeSoa, IndexedSelfAppend, testing::Bool());

TEST(NativeSoa, PermutationAndSortingKeepNestedRowsAlignedAndRestoreScratch) {
    SingleRows owner;
    owner.set_num(4);
    std::uint32_t column_index{};
    owner.get_view().each_column([&](auto column) {
        using Element = typename decltype(column)::value_type;
        for (std::uint32_t row{}; row < 4; ++row) {
            column[row] = static_cast<Element>(column_index * 100 + row);
        }
        ++column_index;
    });
    std::array const keys{3, 1, 3, 2};
    std::ranges::copy(keys, owner.values().begin());
    auto const capacity{owner.capacity()};
    auto verify = [&](std::array<std::uint32_t, 4> const& order) {
        EXPECT_EQ(owner.num(), order.size());
        EXPECT_EQ(owner.capacity(), capacity);
        column_index = 0;
        owner.get_const_view().each_column([&](auto column) {
            using Element = typename decltype(column)::value_type;
            for (std::uint32_t row{}; row < 4; ++row) {
                auto const expected{column_index == 0
                                        ? keys[order[row]]
                                        : static_cast<int>(column_index * 100 + order[row])};
                EXPECT_EQ(column[row], static_cast<Element>(expected));
            }
            ++column_index;
        });
    };

    std::array<std::uint32_t, 4> permutation{2, 0, 3, 1};
    auto const original_permutation{permutation};
    owner.apply_permutation(permutation);
    EXPECT_EQ(permutation, original_permutation);
    verify({2, 0, 3, 1});

    std::array<std::uint32_t, 4> scratch{};
    owner.stable_sort(
        [](auto const& rows, std::uint32_t left, std::uint32_t right) {
            return rows.values()[left] < rows.values()[right];
        },
        scratch);
    verify({1, 3, 2, 0});
    EXPECT_EQ(scratch, (std::array<std::uint32_t, 4>{3, 2, 0, 1}));

    owner.sort(
        [](auto const& rows, std::uint32_t left, std::uint32_t right) {
            auto const values{rows.values()};
            auto const ordinals{rows.view_positions().xs()};
            return values[left] != values[right] ? values[left] > values[right]
                                                 : ordinals[left] < ordinals[right];
        },
        scratch);
    verify({0, 2, 3, 1});
    EXPECT_EQ(scratch, (std::array<std::uint32_t, 4>{3, 2, 1, 0}));

    owner.reset();
    owner.apply_permutation({});
    owner.sort([](auto const&, auto, auto) { return false; }, {});
    EXPECT_TRUE(owner.is_empty());
}

struct ApiSource {
    using soa_schema = ApiOwner::soa_schema;
    struct Positions {
        std::array<float, 2> x{2.0f, 4.0f};
        std::array<float, 2> y{3.0f, 5.0f};
        auto xs() const -> std::span<float const> { return x; }
        auto ys() const -> std::span<float const> { return y; }
    } positions;
    std::array<float, 2> value_columns{11.0f, 13.0f};
    std::array<ApiMask, 2> mask_columns{};
    auto num() const -> std::uint32_t { return 2; }
    void validate() const {}
    auto values() const -> std::span<float const> { return value_columns; }
    auto masks() const -> std::span<ApiMask const> { return mask_columns; }
    auto view_positions() const -> Positions const& { return positions; }
};

TEST(NativeSoa, LogicalApiAcceptsIndependentColumnsAndCompactSourcesDuringGrowth) {
    ApiSource source;
    source.mask_columns[1].set(ApiField::Values);
    ApiOwner owner{};
    owner.append_from(source);
    EXPECT_EQ(owner.total(), 24.0f);
    owner.set_num(owner.capacity());
    auto const compact{owner.get_const_view().left(2)};
    auto const first{owner.append_from(compact)};
    EXPECT_EQ(owner.get_const_view().values()[first + 1], 13.0f);
    EXPECT_EQ(owner.get_const_view().view_positions()[first + 1].y, 5.0f);
    EXPECT_TRUE(owner.get_const_view().masks()[first + 1].has(ApiField::Values));
    EXPECT_EQ(owner.get_const_view().first_x(), 2.0f);
}

TEST(NativeSoa, CompactSelfCopyPreservesEveryColumn) {
    struct CopyRange {
        char const* name;
        std::uint32_t source;
        std::uint32_t destination;
        std::uint32_t count;
        bool single_element{};
    };
    constexpr CopyRange cases[]{
        {"source before destination", 0, 2, 5},
        {"destination before source", 2, 0, 5},
        {"identical ranges", 1, 1, 5},
        {"disjoint forward", 0, 5, 3},
        {"disjoint backward", 5, 0, 3},
        {"one element", 3, 6, 1, true},
        {"identical element", 3, 3, 1, true},
        {"empty range", 2, 4, 0},
    };
    constexpr std::uint32_t row_count{8};
    for (auto const& range : cases) {
        SCOPED_TRACE(range.name);
        for (bool const sliced : {false, true}) {
            SCOPED_TRACE(sliced);
            SingleRows owner{};
            owner.set_num(row_count);
            auto const view{owner.get_view()};
            std::uint32_t column_index{};
            view.each_column([&](auto column) {
                using Element = std::remove_cvref_t<decltype(column[0])>;
                for (std::uint32_t row{}; row < row_count; ++row) {
                    column[row] = static_cast<Element>(column_index * 16 + row);
                }
                ++column_index;
            });
            auto const capacity{owner.capacity()};
            auto const source_first{sliced ? range.source : 0};
            auto const source{owner.get_const_view(source_first, row_count - source_first)};
            if (range.single_element) {
                owner.copy_element(range.destination, source, range.source - source_first);
            } else {
                owner.copy_elements(
                    range.destination, source, range.source - source_first, range.count);
            }

            EXPECT_EQ(owner.num(), row_count);
            EXPECT_EQ(owner.capacity(), capacity);
            column_index = 0;
            view.each_column([&](auto const column) {
                using Element = std::remove_cvref_t<decltype(column[0])>;
                SCOPED_TRACE(column_index);
                for (std::uint32_t row{}; row < row_count; ++row) {
                    auto const copied{row >= range.destination &&
                                      row < range.destination + range.count};
                    auto const original{copied ? range.source + row - range.destination : row};
                    EXPECT_EQ(column[row], static_cast<Element>(column_index * 16 + original));
                }
                ++column_index;
            });
            EXPECT_EQ(column_index, 7);
        }
    }
}

TEST(NativeSoa, LayoutGrowthAndMoves) {
    SingleAlignmentRows owner{};
    EXPECT_EQ(owner.num(), 0);
    EXPECT_EQ(owner.capacity(), 0);
    EXPECT_EQ(owner.get_view().bytes().data(), nullptr);
    for (std::uint32_t const count : {1, 3, 17, 63, 64, 65, 127, 128, 129, 4097}) {
        auto const previous{owner.num()};
        owner.set_num(count);
        EXPECT_EQ(owner.num(), count);
        EXPECT_GE(owner.capacity(), count);
        EXPECT_EQ(owner.capacity() % 64, 0);
        auto const view{owner.get_view()};
        EXPECT_EQ(view.aligned32().back().value, 32);
        EXPECT_EQ(view.aligned64().back().value, 64);
        EXPECT_EQ(view.aligned256().back().value, 256);
        auto const nested{view.view_nested().view_starts()};
        EXPECT_EQ(nested.xs().back(), 0.f);
        for (std::uint32_t i{}; i < previous; ++i) {
            EXPECT_EQ(nested.ys()[i], static_cast<float>(i));
        }
        for (std::uint32_t i{}; i < count; ++i) {
            nested.ys()[i] = static_cast<float>(i);
        }
        std::uintptr_t previous_end{};
        auto const base{reinterpret_cast<std::uintptr_t>(view.bytes().data())};
        view.each_column([&](auto column) {
            using T = typename decltype(column)::value_type;
            auto const address{reinterpret_cast<std::uintptr_t>(column.data())};
            EXPECT_EQ(address % std::max(std::size_t{64}, alignof(T)), 0);
            auto const cursor{previous_end == 0 ? base : previous_end + 192};
            auto const alignment{std::max(std::size_t{64}, alignof(T))};
            EXPECT_EQ(address - base, ((cursor - base + alignment - 1) / alignment) * alignment);
            previous_end = address + static_cast<std::size_t>(owner.capacity()) * sizeof(T);
            EXPECT_LE(previous_end, base + owner.allocated_bytes());
        });
        EXPECT_EQ(previous_end - base, owner.allocated_bytes());
    }
    auto const capacity{owner.capacity()};
    owner.reserve(capacity);
    EXPECT_EQ(owner.capacity(), capacity);
    owner.set_num(129);
    owner.remove_at_swap(3, 2);
    EXPECT_EQ(owner.view_nested().view_starts().ys()[3], 127.f);
    EXPECT_EQ(owner.num(), 127);
    auto const* data{owner.get_view().bytes().data()};
    auto moved{std::move(owner)};
    // NOLINTNEXTLINE(bugprone-use-after-move): Verify the defined moved-from state.
    EXPECT_EQ(owner.capacity(), 0);
    EXPECT_EQ(moved.get_view().bytes().data(), data);
    owner.reserve(64);
    owner = std::move(moved);
    EXPECT_EQ(owner.get_view().bytes().data(), data);
    // NOLINTNEXTLINE(bugprone-use-after-move): Verify the defined moved-from state.
    EXPECT_EQ(moved.num(), 0);
    EXPECT_EQ(owner.get_view(2, 3).num(), 3);
    owner.reset();
    EXPECT_EQ(owner.num(), 0);
    EXPECT_EQ(owner.capacity(), capacity);
    owner.add_uninitialised(63);
    EXPECT_EQ(owner.get_view().bytes().data(), data);
    owner.get_view().bytes()[0] = 42;
    EXPECT_EQ(std::as_const(owner).get_view().bytes()[0], 42);
}

TEST(NativeSoa, PhysicalColumnsStaySynchronizedAcrossGrowthAndReset) {
    SingleRows owner;
    for (std::uint32_t pass{}; pass < 20; ++pass) {
        owner.add_defaulted(129);
        EXPECT_EQ(owner.num(), 129U * (pass + 1));
    }
    std::size_t columns{};
    std::size_t row_bytes{};
    owner.get_view().each_column([&](auto column) {
        ++columns;
        row_bytes += sizeof(typename decltype(column)::value_type);
        EXPECT_EQ(column.size(), owner.num());
    });
    EXPECT_EQ(columns, 7U);
    EXPECT_EQ(row_bytes, sizeof(std::int32_t) + 6 * sizeof(float));
    owner.values()[0] = 17;
    auto const capacity{owner.capacity()};
    owner.reset();
    owner.set_num(65);
    EXPECT_EQ(owner.capacity(), capacity);
    EXPECT_EQ(owner.values()[0], 0);
}

TEST(NativeSoa, SwapRemovalHandlesEveryBoundaryCase) {
    auto const check = [](std::uint32_t const index,
                          std::uint32_t const count,
                          std::span<std::int32_t const> const expected) {
        SingleRows owner;
        owner.set_num(6);
        auto columns{owner.get_view()};
        for (std::uint32_t row{}; row < owner.num(); ++row) {
            columns.values()[row] = row;
            columns.view_positions().xs()[row] = static_cast<float>(row);
        }

        owner.remove_at_swap(index, count);

        ASSERT_EQ(owner.num(), expected.size());
        columns = owner.get_view();
        std::uint32_t expected_index{};
        for (auto const expected_value : expected) {
            EXPECT_EQ(columns.values()[expected_index], expected_value);
            EXPECT_EQ(columns.view_positions().xs()[expected_index],
                      static_cast<float>(expected_value));
            ++expected_index;
        }
    };
    std::array const unchanged{0, 1, 2, 3, 4, 5};
    std::array const one_removed{0, 1, 5, 3, 4};
    std::array const beginning_removed{4, 5, 2, 3};
    std::array const end_removed{0, 1, 2, 3, 4};
    std::array const trailing_removed{0, 1, 5};
    std::array<std::int32_t, 0> const all_removed;

    check(0, 0, unchanged);
    check(2, 1, one_removed);
    check(0, 2, beginning_removed);
    check(5, 1, end_removed);
    check(2, 3, trailing_removed);
    check(0, 6, all_removed);
}

TEST(NativeSoa, CheckedCapacityArithmetic) {
    using Storage = SingleRows;
    std::uint32_t result{};
    EXPECT_FALSE(native_soa::try_round_capacity(-1, Storage::max_capacity, result));
    EXPECT_FALSE(native_soa::try_round_capacity(
        std::int64_t{Storage::max_capacity} + 1, Storage::max_capacity, result));
    EXPECT_TRUE(native_soa::try_round_capacity(65, Storage::max_capacity, result));
    EXPECT_EQ(result, 128);
    EXPECT_LE(Storage::layout_bytes(2), 2 * Storage::capacity_block_bound);
}

TEST(NativeSoa, ColumnOffsetsMatchSequentialLayoutAcrossAlignments) {
    using namespace single_allocation_layout;
    struct alignas(256) Overaligned {
        std::byte bytes[256];
    };
    using Layout =
        ColumnLayouts<std::int32_t, std::int32_t, Overaligned, std::byte, Overaligned, float>;
    static_assert(Layout::metadata.first_non_linear == 4);
    static_assert(Layout::allocation_alignment == 256);

    constexpr auto copied{[] {
        auto metadata{Layout::metadata};
        return metadata;
    }()};
    static_assert(copied.columns[4].block_offset == Layout::metadata.columns[4].block_offset);
    static_assert(copied.columns[4].gap_offset == Layout::metadata.columns[4].gap_offset);
    static_assert(copied.offset(4, 3) == Layout::offset(4, 3));

    for (std::size_t const blocks : {0, 1, 2, 3, 16, 1024}) {
        SCOPED_TRACE(blocks);
        LayoutCursor cursor{blocks};
        std::size_t next{};
        auto check = [&]<typename T>(std::size_t index) {
            auto const alignment{std::max(std::size_t{64}, alignof(T))};
            auto const expected{((next + alignment - 1) / alignment) * alignment};
            EXPECT_EQ(Layout::offset(index, blocks), expected);
            EXPECT_EQ(cursor.advance(sizeof(T), alignof(T)), expected);
            EXPECT_EQ(Layout::metadata.columns[index].alignment, alignment);
            EXPECT_EQ(Layout::metadata.columns[index].element_size, sizeof(T));
            next = expected + blocks * 64 * sizeof(T) + 192;
        };
        check.operator()<std::int32_t>(0);
        check.operator()<std::int32_t>(1);
        check.operator()<Overaligned>(2);
        check.operator()<std::byte>(3);
        check.operator()<Overaligned>(4);
        check.operator()<float>(5);
        EXPECT_EQ(Layout::layout_bytes(blocks), blocks == 0 ? 0 : next - 192);
        EXPECT_LE(Layout::layout_bytes(blocks), blocks * Layout::capacity_block_bound);
    }
}

template <typename Owner>
void check_layout_limits() {
    using namespace single_allocation_layout;
    auto const maximum{Owner::max_capacity};
    EXPECT_EQ(maximum % capacity_granularity, 0);
    std::uint32_t rounded{};
    EXPECT_TRUE(try_round_capacity(maximum, maximum, rounded));
    EXPECT_EQ(rounded, maximum);
    EXPECT_FALSE(try_round_capacity(std::int64_t{maximum} + 1, maximum, rounded));
    EXPECT_FALSE(try_round_capacity(std::numeric_limits<std::int64_t>::max(), maximum, rounded));
    for (auto const capacity : {0u, 64u, 128u, maximum - 64, maximum}) {
        std::size_t expected{};
        typename Owner::ConstView{}.each_column([&](auto column) {
            using T = typename decltype(column)::value_type;
            if (expected != 0) {
                expected += 192;
            }
            auto const alignment{std::max(std::size_t{64}, alignof(T))};
            expected = ((expected + alignment - 1) / alignment) * alignment;
            expected += static_cast<std::size_t>(capacity) * sizeof(T);
        });
        auto const blocks{static_cast<std::size_t>(capacity / capacity_granularity)};
        EXPECT_EQ(Owner::layout_bytes(blocks), expected);
        EXPECT_LE(expected, static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()));
        EXPECT_LE(expected, blocks * Owner::capacity_block_bound);
    }
}

TEST(NativeSoa, IndexedLayoutsPreserveEveryNestedPhysicalColumn) {
    using namespace single_allocation_layout;
    using Layout = SingleAlignmentRows::Layout;
    static_assert(std::is_trivially_copyable_v<Layout>);
    static_assert(Layout::num_columns == 12);

    for (std::size_t const blocks : {0, 1, 2, 3, 16, 1024}) {
        SCOPED_TRACE(blocks);
        std::size_t next{};
        std::size_t count{};
        std::size_t alignment{1};
        auto check = [&]<typename T>(std::size_t index) {
            auto const column_alignment{std::max(std::size_t{64}, alignof(T))};
            auto const expected{((next + column_alignment - 1) / column_alignment) *
                                column_alignment};
            EXPECT_EQ(Layout::offset(index, blocks), expected);
            EXPECT_EQ(Layout::metadata.columns[index].alignment, column_alignment);
            EXPECT_EQ(Layout::metadata.columns[index].element_size, sizeof(T));
            EXPECT_EQ(index, count++);
            alignment = std::max(alignment, column_alignment);
            next = expected + blocks * 64 * sizeof(T) + 192;
        };
        check.operator()<std::uint8_t>(Layout::bytes_column);
        check.operator()<OddBytes>(Layout::odd_first_column);
        check.operator()<OddBytes>(Layout::odd_second_column);
        check.operator()<Aligned32>(Layout::aligned32_column);
        check.operator()<float>(Layout::nested_starts_xs_column);
        check.operator()<float>(Layout::nested_starts_ys_column);
        check.operator()<float>(Layout::nested_starts_zs_column);
        check.operator()<float>(Layout::nested_ends_xs_column);
        check.operator()<float>(Layout::nested_ends_ys_column);
        check.operator()<float>(Layout::nested_ends_zs_column);
        check.operator()<Aligned64>(Layout::aligned64_column);
        check.operator()<Aligned256>(Layout::aligned256_column);
        EXPECT_EQ(count, Layout::num_columns);
        EXPECT_EQ(alignment, Layout::allocation_alignment);
        EXPECT_EQ(Layout::layout_bytes(blocks), blocks == 0 ? 0 : next - 192);
    }
}

TEST(NativeSoa, GeneratedLayoutLimits) {
    check_layout_limits<SingleRows>();
    check_layout_limits<SingleAlignmentRows>();
    std::size_t bytes{17};
    EXPECT_FALSE(
        native_soa::try_allocation_bytes(64, std::numeric_limits<std::size_t>::max(), bytes));
    EXPECT_EQ(bytes, 17);
    EXPECT_FALSE(native_soa::try_allocation_bytes(65, 256, bytes));
}

TEST(NativeSoa, ColumnViewContracts) {
    using View = Vectors::View;
    using ConstView = Vectors::ConstView;
    static_assert(soa_storage_detail::validate_compact_view<View>());
    static_assert(soa_storage_detail::validate_compact_view<ConstView>());
    static_assert(std::is_convertible_v<View, ConstView>);
    static_assert(!std::is_convertible_v<ConstView, View>);
    View empty;
    EXPECT_TRUE(empty.is_empty());
    EXPECT_EQ(empty.num(), 0);
    empty.each_column([](auto column) {
        EXPECT_TRUE(column.empty());
        EXPECT_EQ(column.size(), 0);
        EXPECT_EQ(column.data(), nullptr);
        EXPECT_EQ(column.begin(), column.end());
    });
    EXPECT_EQ(empty.slice(0, 0).zs().data(), nullptr);
    Vectors owner;
    owner.set_num(4);
    auto const view{owner.get_view()};
    auto const resolved{view.resolve()};
    EXPECT_EQ(view.xs().begin() + 4, view.xs().end());
    EXPECT_EQ(view.right(0).zs().data(), view.zs().data() + 4);
    EXPECT_TRUE(view.right(0).zs().empty());
    auto const slice{resolved.slice(1, 2)};
    EXPECT_EQ(slice.xs().data(), view.xs().data() + 1);
    EXPECT_EQ(slice.ys().data(), view.ys().data() + 1);
    EXPECT_EQ(slice.zs().data(), view.zs().data() + 1);
    slice.zs()[1] = 19.f;
    EXPECT_EQ(view.zs()[2], 19.f);
    EXPECT_EQ(slice.get_const_view().zs()[1], 19.f);
    EXPECT_EQ(slice.num(), 2);
    EXPECT_EQ(resolved.right(0).zs().data(), view.zs().data() + 4);
    EXPECT_EQ(empty.resolve().slice(0, 0).zs().data(), nullptr);
}

TEST(NativeSoa, OddSizedGroupsPreservePaddedStridesAcrossGrowth) {
    static_assert(sizeof(odd_pairs::View) == 24);
    static_assert(std::is_trivially_copyable_v<odd_pairs::View>);
    SingleAlignmentRows owner;
    owner.set_num(1);
    auto const initial{owner.view_odd()};
    std::memset(initial.first().data(), 17, sizeof(initial.first()[0]));
    std::memset(initial.second().data(), 31, sizeof(initial.second()[0]));

    for (auto count : {1U, 65U, 193U}) {
        owner.set_num(count);
        auto const view{owner.view_odd()};
        auto const blocks{owner.capacity() / SingleAlignmentRows::capacity_granularity};
        using Layout = SingleAlignmentRows::Layout;
        auto const stride{Layout::offset(Layout::odd_second_column, blocks) -
                          Layout::offset(Layout::odd_first_column, blocks)};
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(view.second().data()) -
                      reinterpret_cast<std::uintptr_t>(view.first().data()),
                  stride);
        EXPECT_EQ(std::as_bytes(view.first())[0], std::byte{17});
        EXPECT_EQ(std::as_bytes(view.second())[0], std::byte{31});

        auto const slice{view.get_const_view().slice(count - 1, 1)};
        EXPECT_EQ(slice.first().data(), view.first().data() + count - 1);
        EXPECT_EQ(slice.second().data(), view.second().data() + count - 1);
        EXPECT_EQ(slice.num(), 1U);
        auto const resolved{slice.resolve()};
        EXPECT_EQ(resolved.first().data(), slice.first().data());
        EXPECT_EQ(resolved.second().data(), slice.second().data());
        auto const parent{owner.get_const_view().slice(count - 1, 1).resolve()};
        EXPECT_EQ(parent.aligned256().data(), owner.aligned256().data() + count - 1);
    }
}

TEST(NativeSoa, HomogeneousNestedGroupsReuseViewsAndPhysicalColumnOrder) {
    SingleAlignmentRows owner;
    owner.set_num(3);
    auto const view{owner.view_nested()};
    static_assert(sizeof(decltype(view)) == 16);
    static_assert(std::is_same_v<decltype(view.view_starts()), vectors::View>);
    static_assert(std::is_same_v<decltype(view.view_ends()), vectors::View>);
    float ordinal{};
    view.each_column([&](auto&& column) {
        static_assert(std::is_rvalue_reference_v<decltype(column)>);
        std::ranges::fill(column, ++ordinal);
    });

    auto const starts{view.view_starts()};
    auto const ends{view.view_ends()};
    EXPECT_EQ(starts.xs()[1], 1.0f);
    EXPECT_EQ(starts.ys()[1], 2.0f);
    EXPECT_EQ(starts.zs()[1], 3.0f);
    EXPECT_EQ(ends.xs()[1], 4.0f);
    EXPECT_EQ(ends.ys()[1], 5.0f);
    EXPECT_EQ(ends.zs()[1], 6.0f);
    auto const slice{view.get_const_view().slice(1, 1)};
    EXPECT_EQ(slice.view_starts().ys().data(), starts.ys().data() + 1);
    EXPECT_EQ(slice.view_ends().zs().data(), ends.zs().data() + 1);
    EXPECT_EQ(slice.view_ends().zs()[0], 6.0f);
}

TEST(NativeSoa, CompactViewsAndBulkAppend) {
    using Owner = SingleRows;
    using Positions = decltype(std::declval<Owner::View>().view_positions());
    using ConstPositions = decltype(std::declval<Owner::ConstView>().view_positions());
    using Velocities = decltype(std::declval<Owner::View>().view_velocities());
    static_assert(sizeof(Positions) == 16);
    static_assert(std::is_trivially_copyable_v<Positions>);
    static_assert(std::is_convertible_v<Positions, ConstPositions>);
    static_assert(!std::is_convertible_v<ConstPositions, Positions>);
    static_assert(std::is_same_v<Velocities, Positions>);
    static_assert(std::is_same_v<Positions, Vectors::View>);
    static_assert(std::is_same_v<ConstPositions, Vectors::ConstView>);
    static_assert(std::is_same_v<decltype(std::declval<Positions>().slice(0, 0)), Positions>);
    static_assert(!std::is_convertible_v<Owner::View, ConstPositions>);
    static_assert(std::is_same_v<decltype(std::declval<Positions>().xs()), std::span<float>>);
    static_assert(
        std::is_same_v<decltype(std::declval<ConstPositions>().xs()), std::span<float const>>);
    static_assert(soa_storage_detail::validate_compact_view<Owner::View>());
    static_assert(soa_storage_detail::validate_compact_view<Owner::ConstView>());
    static_assert(std::is_same_v<decltype(std::declval<Owner::View>().view_positions().resolve()),
                                 vectors::ResolvedView>);
    static_assert(
        std::is_same_v<decltype(std::declval<Owner::ConstView>().view_positions().resolve()),
                       vectors::ResolvedConstView>);
    Owner source{};
    source.set_num(129);
    auto view{source.get_view()};
    auto const row_count{source.num()};
    for (std::uint32_t row{}; row < row_count; ++row) {
        view.values()[row] = row;
        view.view_positions().xs()[row] = static_cast<float>(row);
    }
    source.reserve(1024);
    view = source.get_view();
    auto const slice{view.slice(1, 64)};
    ConstPositions const const_positions = slice.view_positions();
    EXPECT_EQ(const_positions.xs().data(), view.view_positions().xs().data() + 1);
    EXPECT_EQ(const_positions.num(), 64);
    static_assert(std::is_same_v<decltype(const_positions.get_const_view()), ConstPositions>);
    static_assert(std::is_same_v<decltype(const_positions.get_const_view(1, 1)), ConstPositions>);
    EXPECT_EQ(const_positions.get_const_view().xs().data(), const_positions.xs().data());
    EXPECT_EQ(const_positions.get_const_view(1, 1).xs()[0], 2.0f);
    EXPECT_EQ(slice.values()[63], 64);
    EXPECT_EQ(view.view_positions().xs()[1], 1.f);
    Owner::ConstView const const_view{slice};
    EXPECT_EQ(const_view.view_positions().xs()[0], 1.f);
    static_assert(std::is_same_v<decltype(const_view.get_const_view()), Owner::ConstView>);
    static_assert(std::is_same_v<decltype(const_view.get_const_view(1, 1)), Owner::ConstView>);
    EXPECT_EQ(const_view.get_const_view().values().data(), const_view.values().data());
    EXPECT_EQ(const_view.get_const_view(1, 1).view_positions().xs()[0], 2.0f);
    Owner destination{};
    EXPECT_EQ(destination.append_from(source), 0);
    EXPECT_EQ(destination.append_from(const_view), 129);
    EXPECT_EQ(destination.get_view().values()[192], 64);
    auto const first{destination.num()};
    EXPECT_EQ(destination.append_from(destination), first);
    EXPECT_EQ(destination.num(), 2 * first);
    EXPECT_EQ(destination.values()[first + 64], 64);
    EXPECT_EQ(destination.view_positions().xs()[first + 64], 64.f);
    auto self{destination.slice(63, 65)};
    destination.append_from(self);
    EXPECT_EQ(destination.get_view().values()[2 * static_cast<std::size_t>(first)], 63);
    EXPECT_EQ(destination.append_from(destination.left(0)), destination.num());
    source.get_view().values()[0] = -1;
    EXPECT_EQ(destination.get_view().values()[0], 0);
}

TEST(NativeSoa, BulkAppendPreservesEveryAlignedLeaf) {
    SingleAlignmentRows source{};
    source.set_num(129);
    source.get_view().each_column([](auto column) {
        for (std::size_t row{}; row < column.size(); ++row) {
            std::memset(&column[row], static_cast<int>(row + 1), sizeof(column[row]));
        }
    });
    SingleAlignmentRows destination{};
    destination.append_from(source.get_const_view());
    destination.append_from(destination.slice(1, 128));
    destination.get_const_view().each_column([](auto column) {
        using Element = typename decltype(column)::value_type;
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(column.data()) % alignof(Element), 0u);
        for (std::size_t row{}; row < column.size(); ++row) {
            auto const expected{static_cast<unsigned char>(row < 129 ? row + 1 : row - 127)};
            auto const bytes{std::as_bytes(column.subspan(row, 1))};
            for (auto byte : bytes) {
                EXPECT_EQ(std::to_integer<unsigned char>(byte), expected);
            }
        }
    });
}

struct IndependentRows {
    using soa_schema = SingleRows::soa_schema;
    std::array<std::int32_t, 65> values_{};
    std::array<float, 65> xs_{}, ys_{}, zs_{};
    auto num() const -> std::uint32_t { return 65; }
    void validate() const {}
    auto values() const { return std::span{values_}; }
    auto xs() const { return std::span{xs_}; }
    auto ys() const { return std::span{ys_}; }
    auto zs() const { return std::span{zs_}; }
    auto view_positions() const -> IndependentRows const& { return *this; }
    auto view_velocities() const -> IndependentRows const& { return *this; }
};

TEST(NativeSoa, IndependentColumnsAppendDirectly) {
    IndependentRows source;
    for (std::uint32_t row{}; row < source.num(); ++row) {
        source.values_[row] = row + 100;
        source.xs_[row] = static_cast<float>(row);
    }
    SingleRows destination{};
    destination.set_num(1);
    destination.get_view().values()[0] = -1;
    EXPECT_EQ(destination.append_from(source), 1);
    EXPECT_EQ(destination.num(), 66);
    EXPECT_EQ(destination.get_const_view().values()[0], -1);
    EXPECT_EQ(destination.get_const_view().values()[65], 164);
    EXPECT_EQ(destination.get_const_view().view_positions().xs()[65], 64.f);
    EXPECT_EQ(source.values_[64], 164);
    destination.reset();
    EXPECT_EQ(destination.append_from(source, 1, 16), 0);
    EXPECT_EQ(destination.append_from(source, 0, 0), 16);
    EXPECT_EQ(destination.append_from(source, 17, 16), 16);
    EXPECT_EQ(destination.num(), 32);
    EXPECT_EQ(destination.get_const_view().values()[0], 101);
    EXPECT_EQ(destination.get_const_view().values()[31], 132);
    std::array<std::uint32_t, 3> const indices{64, 0, 32};
    EXPECT_EQ(destination.append_from(source, indices), 32U);
    EXPECT_EQ(destination.num(), 35U);
    EXPECT_EQ(destination.values()[32], 164);
    EXPECT_EQ(destination.values()[33], 100);
    EXPECT_EQ(destination.values()[34], 132);
    EXPECT_EQ(destination.view_positions().xs()[32], 64.f);
    EXPECT_EQ(destination.view_positions().xs()[33], 0.f);
    EXPECT_EQ(destination.view_positions().xs()[34], 32.f);
}

TEST(NativeSoa, DescendingRemovalExhaustiveSubsets) {
    for (std::uint32_t count{}; count <= 10; ++count) {
        for (unsigned mask{}; mask < (1u << count); ++mask) {
            SingleRows owner{};
            owner.set_num(count);
            auto view{owner.get_view()};
            std::vector<std::uint32_t> indices;
            for (auto remaining{count}; remaining > 0; --remaining) {
                auto const row{remaining - 1};
                view.values()[row] = row;
                view.view_positions().xs()[row] = static_cast<float>(row);
                if (mask & (1u << row)) {
                    indices.push_back(row);
                }
            }
            auto const final_count{count - static_cast<std::uint32_t>(indices.size())};
            std::vector<std::int32_t> tail;
            for (auto row{final_count}; row < count; ++row) {
                if (!(mask & (1u << row))) {
                    tail.push_back(row);
                }
            }
            std::vector<std::int32_t> expected;
            std::size_t next{};
            for (std::uint32_t row{}; row < final_count; ++row) {
                expected.push_back(mask & (1u << row) ? tail[next++] : row);
            }
            auto const capacity{owner.capacity()};
            owner.remove_at_swap(std::span<std::uint32_t const>{indices});
            EXPECT_EQ(owner.capacity(), capacity);
            EXPECT_TRUE(std::ranges::equal(owner.get_view().values(), expected));
            for (std::uint32_t row{}; row < final_count; ++row) {
                EXPECT_EQ(owner.get_view().view_positions().xs()[row],
                          static_cast<float>(expected[row]));
            }
        }
    }
}

}
