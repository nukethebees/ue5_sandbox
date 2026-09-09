#include "single_allocation_soa_spacing_support.h"
#include "Windows/WindowsHWrapper.h"

namespace ml::soa_spacing_experiment {
void initialise(SpacingDoublesView view) {
    each_leaf(view, [](auto column) {
        for (auto& value : column) {
            value = 2.;
        }
    });
}
void initialise(SpacingMixedWidthsView view) {
    auto initialise_bundle = [](SpacingMixedBundleView bundle) {
        auto const count{bundle.num()};
        for (int32 row{}; row < count; ++row) {
            bundle.flags[row] = 1;
            bundle.counters[row] = 65534;
            bundle.counts[row] = 3;
            bundle.totals[row] = 4;
            bundle.payloads[row] = {{1, 2, 3}};
            bundle.values[row] = 1.f;
            bundle.rates[row] = 2.;
        }
    };
    view.apply_arrays([&](auto... bundles) { (initialise_bundle(bundles), ...); });
}
void initialise(SpacingAlignedView view) {
    view.apply_arrays([](auto... bundles) {
        auto initialise_bundle = [](SpacingAlignedBundleView bundle) {
            for (auto& value : bundle.velocities) {
                value = 2.f;
            }
        };
        (initialise_bundle(bundles), ...);
    });
}
FORCEINLINE void update(SpacingDoubleVectorView positions, SpacingDoubleVectorView velocities, int32 row) {
    positions.xs[row] += velocities.xs[row] * 0.125;
    positions.ys[row] += velocities.ys[row] * 0.125;
    positions.zs[row] += velocities.zs[row] * 0.125;
}
FORCENOINLINE void iterate(SpacingDoublesView view, bool wide) {
    auto const count{view.num()};
    for (int32 pass{}; pass < 4; ++pass) {
        for (int32 row{}; row < count; ++row) {
            update(view.positions0, view.velocities0, row);
            if (wide) {
                update(view.positions1, view.velocities1, row);
                update(view.positions2, view.velocities2, row);
                update(view.positions3, view.velocities3, row);
            }
        }
    }
}
FORCEINLINE void update(SpacingMixedBundleView bundle, int32 row) {
    bundle.flags[row] ^= 1;
    bundle.counters[row] = static_cast<uint16>(bundle.counters[row] + 1u);
    bundle.counts[row] += bundle.counters[row];
    bundle.totals[row] += bundle.counts[row];
    bundle.payloads[row].bytes[0] = static_cast<uint8>(bundle.payloads[row].bytes[0] + bundle.flags[row] + 1u);
    bundle.values[row] += 0.125f;
    bundle.rates[row] += bundle.values[row] * 0.125;
}
FORCENOINLINE void iterate(SpacingMixedWidthsView view, bool wide) {
    auto const count{view.num()};
    for (int32 pass{}; pass < 4; ++pass) {
        for (int32 row{}; row < count; ++row) {
            update(view.bundle0, row);
            if (wide) {
                update(view.bundle1, row);
                update(view.bundle2, row);
                update(view.bundle3, row);
                update(view.bundle4, row);
                update(view.bundle5, row);
            }
        }
    }
}
FORCEINLINE void update(SpacingAlignedBundleView bundle, int32 row) {
    bundle.positions[row] += bundle.velocities[row] * 0.125f;
    bundle.aligned32[row].value = (bundle.aligned32[row].value & 255) + 1;
    bundle.aligned64[row].value = (bundle.aligned64[row].value & 255) + 3;
    bundle.aligned256[row].value = (bundle.aligned256[row].value & 255) + 5;
}
FORCENOINLINE void iterate(SpacingAlignedView view, bool wide) {
    auto const count{view.num()};
    for (int32 pass{}; pass < 4; ++pass) {
        for (int32 row{}; row < count; ++row) {
            update(view.bundle0, row);
            if (wide) {
                update(view.bundle1, row);
                update(view.bundle2, row);
            }
        }
    }
}

auto valid_processor(int32 group, int32 cpu) -> bool {
    return group >= 0 && group < GetActiveProcessorGroupCount() && cpu >= 0 && cpu < 64 &&
           static_cast<DWORD>(cpu) < GetActiveProcessorCount(static_cast<WORD>(group));
}
class PinnedCpu {
  public:
    PinnedCpu(int32 group, int32 cpu)
        : group_{group}
        , cpu_{cpu} {
        REQUIRE(valid_processor(group, cpu));
        GROUP_AFFINITY selected{};
        selected.Group = static_cast<WORD>(group);
        selected.Mask = KAFFINITY{1} << cpu;
        auto const success{SetThreadGroupAffinity(GetCurrentThread(), &selected, &previous_)};
        INFO("SetThreadGroupAffinity error: " << (success ? 0 : GetLastError()));
        REQUIRE(success != 0);
    }
    ~PinnedCpu() {
        ensureAlwaysMsgf(SetThreadGroupAffinity(GetCurrentThread(), &previous_, nullptr) != 0,
                         TEXT("Failed to restore benchmark thread affinity"));
    }
    PinnedCpu(PinnedCpu const&) = delete;
    auto operator=(PinnedCpu const&) -> PinnedCpu& = delete;
    void validate_cpu(char const* phase) const {
        GROUP_AFFINITY effective{};
        REQUIRE(GetThreadGroupAffinity(GetCurrentThread(), &effective) != 0);
        REQUIRE(effective.Group == group_);
        REQUIRE(effective.Mask == (KAFFINITY{1} << cpu_));
        PROCESSOR_NUMBER observed{};
        GetCurrentProcessorNumberEx(&observed);
        REQUIRE(observed.Group == group_);
        REQUIRE(observed.Number == cpu_);
        std::printf("\nSPACING_CPU,%s,%d,%d,%u,%u\n",
                    phase,
                    group_,
                    cpu_,
                    static_cast<unsigned>(observed.Group),
                    static_cast<unsigned>(observed.Number));
    }
  private:
    GROUP_AFFINITY previous_{};
    int32 group_{}, cpu_{};
};

template <typename View, typename Single, typename Array>
void run_shape(int32 capacity, int32 live, int32 gap, int32 owner, bool wide) {
    if (owner == 0) {
        SpacedOwner<View> storage{capacity, live, static_cast<SIZE_T>(gap)};
        describe(storage.view, storage.data, capacity, storage.bytes);
        measure(storage.view, wide);
        storage.check_gaps();
    } else if (owner == 1) {
        Single storage;
        storage.reserve(capacity);
        storage.add_defaulted(live);
        auto view{storage.get_view().columns()};
        initialise(view);
        std::byte const* base{};
        each_leaf(view, [&](auto column) {
            if (!base) {
                base = reinterpret_cast<std::byte const*>(column.GetData());
            }
        });
        describe(view, base, storage.capacity(), storage.allocated_bytes());
        measure(view, wide);
    } else {
        Array storage;
        storage.reserve(capacity);
        storage.add_defaulted(live);
        auto view{storage.get_view()};
        initialise(view);
        SIZE_T bytes{};
        std::vector<int32> capacities;
        each_leaf(storage, [&](auto const& column) {
            capacities.push_back(column.Max());
            bytes += static_cast<SIZE_T>(column.Max()) * sizeof(column[0]);
        });
        describe(view, nullptr, capacity, bytes, capacities);
        measure(view, wide);
    }
}
TEST_CASE("SandboxCore.SingleAllocation.Spacing.Confirmation", "[benchmark][.]") {
    auto const capacity{parameter(TEXT("SOA_SPACING_CAPACITY"))};
    auto const live{parameter(TEXT("SOA_SPACING_LIVE"))};
    auto const gap{parameter(TEXT("SOA_SPACING_GAP"))};
    auto const owner{parameter(TEXT("SOA_SPACING_OWNER"))};
    auto const schema{parameter(TEXT("SOA_SPACING_SCHEMA"))};
    auto const wide{parameter(TEXT("SOA_SPACING_WIDE")) != 0};
    REQUIRE(live > 0);
    REQUIRE(capacity >= live);
    REQUIRE(capacity <= 131072);
    REQUIRE(capacity % 64 == 0);
    REQUIRE((gap == 0 || gap == 64 || gap == 192));
    REQUIRE(owner >= 0);
    REQUIRE(owner <= 2);
    PinnedCpu pin{parameter(TEXT("SOA_SPACING_CPU_GROUP")), parameter(TEXT("SOA_SPACING_CPU"))};
    pin.validate_cpu("before");
    switch (schema) {
        case 0:
            run_shape<EntityDataView, SingleAllocationEntityData, EntityData>(capacity, live, gap, owner, wide);
            break;
        case 1:
            run_shape<SpacingDoublesView, SingleSpacingDoubles, SpacingDoubles>(capacity, live, gap, owner, wide);
            break;
        case 2:
            run_shape<SpacingMixedWidthsView, SingleSpacingMixedWidths, SpacingMixedWidths>(capacity, live, gap, owner, wide);
            break;
        case 3:
            run_shape<SpacingAlignedView, SingleSpacingAligned, SpacingAligned>(capacity, live, gap, owner, wide);
            break;
        default:
            FAIL("Unknown spacing schema");
    }
    pin.validate_cpu("after");
}

template <typename View, typename Array>
void check_shape() {
    for (int32 capacity : {65536, 75008, 131072}) {
        for (SIZE_T gap : {0, 64, 192}) {
            for (bool wide : {false, true}) {
                Array reference;
                reference.add_defaulted(17);
                auto expected{reference.get_view()};
                initialise(expected);
                SpacedOwner<View> candidate{capacity, 17, gap};
                SIZE_T end{};
                for (auto const column : candidate.columns) {
                    auto const unaligned{end + (end == 0 ? 0 : gap)};
                    auto const expected_offset{((unaligned + column.alignment - 1) / column.alignment) * column.alignment};
                    REQUIRE(column.offset == expected_offset);
                    REQUIRE(column.bytes == static_cast<SIZE_T>(capacity) * column.element_size);
                    end = column.offset + column.bytes;
                }
                REQUIRE(candidate.bytes == end);
                iterate(expected, wide);
                iterate(candidate.view, wide);
                SIZE_T index{};
                each_leaf(expected, [&](auto column) {
                    using Element = std::remove_cvref_t<decltype(column[0])>;
                    auto* actual{reinterpret_cast<Element*>(candidate.data + candidate.columns[index++].offset)};
                    for (int32 row{}; row < 17; ++row) {
                        if constexpr (requires { actual[row].value; }) {
                            REQUIRE(actual[row].value == column[row].value);
                        } else {
                            REQUIRE(FMemory::Memcmp(&actual[row], &column[row], sizeof(Element)) == 0);
                        }
                    }
                });
                candidate.check_gaps();
            }
        }
    }
}
template <typename View, typename Single>
void check_generated_layout() {
    for (int32 const capacity : {64, 128, 65536, 65600, 75008, 100032, 131072}) {
        SpacedOwner<View> reference{capacity, 1, 192};
        Single generated;
        generated.reserve(capacity);
        generated.add_defaulted(1);
        REQUIRE(generated.allocated_bytes() == reference.bytes);
        auto const columns{generated.get_view().columns()};
        UPTRINT base{};
        SIZE_T ordinal{};
        each_leaf(columns, [&](auto column) {
            auto const address{reinterpret_cast<UPTRINT>(column.GetData())};
            if (ordinal == 0) {
                base = address;
            }
            REQUIRE(address - base == reference.columns[ordinal++].offset);
        });
    }
}
TEST_CASE("SandboxCore.SingleAllocation.Spacing.ShapeCorrectness") {
    check_generated_layout<EntityDataView, SingleAllocationEntityData>();
    check_generated_layout<SpacingDoublesView, SingleSpacingDoubles>();
    check_generated_layout<SpacingMixedWidthsView, SingleSpacingMixedWidths>();
    check_generated_layout<SpacingAlignedView, SingleSpacingAligned>();
    check_generated_layout<AlignmentDataView, SingleAllocationAlignmentData>();
    check_shape<SpacingDoublesView, SpacingDoubles>();
    check_shape<SpacingMixedWidthsView, SpacingMixedWidths>();
    check_shape<SpacingAlignedView, SpacingAligned>();
    SpacedOwner<SpacingMixedWidthsView> mixed{64, 1, 64};
    iterate(mixed.view, true);
    REQUIRE(mixed.view.bundle0.counters[0] == 2);
    REQUIRE(mixed.view.bundle5.values[0] == 1.5f);
    REQUIRE(mixed.view.bundle5.payloads[0].bytes[0] == 7);
    SpacedOwner<SpacingDoublesView> doubles{64, 1, 192};
    iterate(doubles.view, false);
    REQUIRE(doubles.view.positions0.xs[0] == 3.);
    REQUIRE(doubles.view.positions1.xs[0] == 2.);
    SpacedOwner<SpacingAlignedView> aligned{64, 1, 64};
    iterate(aligned.view, true);
    REQUIRE(aligned.view.bundle2.positions[0] == 1.f);
    REQUIRE(aligned.view.bundle2.aligned32[0].value == 36);
    REQUIRE(aligned.view.bundle2.aligned64[0].value == 76);
    REQUIRE(aligned.view.bundle2.aligned256[0].value == 20);
    for (int32 capacity : {65536, 75008, 131072}) {
        SpacedOwner<AlignmentDataView> sequential{capacity, 1, 192};
        auto const generated_bytes{SingleAllocationAlignmentData::layout_bytes(static_cast<SIZE_T>(capacity / 64))};
        REQUIRE(generated_bytes == sequential.bytes);
        sequential.check_gaps();
        std::printf("SPACING_LAYOUT_REFERENCE,AlignmentData,%d,%zu,%zu\n",
                    capacity,
                    static_cast<std::size_t>(generated_bytes),
                    static_cast<std::size_t>(sequential.bytes));
    }
}
TEST_CASE("SandboxCore.SingleAllocation.Spacing.AffinityCorrectness") {
    REQUIRE_FALSE(valid_processor(-1, 0));
    REQUIRE_FALSE(valid_processor(GetActiveProcessorGroupCount(), 0));
    REQUIRE_FALSE(valid_processor(0, -1));
    REQUIRE_FALSE(valid_processor(0, 64));
    GROUP_AFFINITY before{};
    REQUIRE(GetThreadGroupAffinity(GetCurrentThread(), &before) != 0);
    int32 cpu{};
    while ((before.Mask & (KAFFINITY{1} << cpu)) == 0) {
        ++cpu;
    }
    {
        PinnedCpu pin{before.Group, cpu};
        pin.validate_cpu("test");
    }
    GROUP_AFFINITY after{};
    REQUIRE(GetThreadGroupAffinity(GetCurrentThread(), &after) != 0);
    REQUIRE(before.Group == after.Group);
    REQUIRE(before.Mask == after.Mask);
}
}
