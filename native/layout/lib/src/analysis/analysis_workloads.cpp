#include <ioj/layout/analysis/analysis_workloads.hpp>

#include "../access_intents.hpp"
#include "../checked_arithmetic.hpp"
#include "../memory_regions.hpp"
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analysis/analysis_context.hpp>
#include <ioj/layout/analysis/analysis_request.hpp>
#include <ioj/layout/analysis/analysis_storage.hpp>

#include <algorithm>
#include <bit>
#include <cassert>
#include <string>
#include <utility>

namespace ioj::layout {
namespace workload_detail {

using namespace analysis_detail;
using Quantity = std::optional<std::uint64_t>;
using Intervals = std::vector<std::pair<std::uint64_t, std::uint64_t>>;

void error(AnalysisContext& context, std::string message) {
    context.report({DiagnosticSeverity::error, std::move(message)});
}

auto multiply(AnalysisContext& context, Quantity value, std::uint64_t count, std::string_view label)
    -> Quantity {
    if (!value) {
        return std::nullopt;
    }
    auto const result{checked_multiply(*value, count)};
    if (!result) {
        error(context, std::string{label} + " overflows uint64.");
    }
    return result;
}

void accumulate(AnalysisContext& context, Quantity& total, Quantity value, std::string_view label) {
    if (!total) {
        return;
    }
    if (!value) {
        total.reset();
        return;
    }
    total = checked_add(*total, *value);
    if (!total) {
        error(context, std::string{label} + " total overflows uint64.");
    }
}

auto difference(Quantity whole, Quantity part) -> Quantity {
    return whole && part && *whole >= *part ? Quantity{*whole - *part} : std::nullopt;
}

void classify(AnalysisContext& context,
              PayloadFootprint& payload,
              Quantity useful,
              AccessOperation operation,
              std::uint64_t multiplicity) {
    payload.useful = useful;
    payload.read = access_includes_read(operation) ? useful : Quantity{0};
    payload.write = access_includes_write(operation) ? useful : Quantity{0};
    if (multiplicity != 0) {
        payload.logical_read =
            multiply(context, payload.read, multiplicity, "Logical read payload");
        payload.logical_write =
            multiply(context, payload.write, multiplicity, "Logical write payload");
    }
}

void accumulate_payload(AnalysisContext& context,
                        PayloadFootprint& total,
                        PayloadFootprint const& value) {
    accumulate(context, total.useful, value.useful, "Useful payload");
    accumulate(context, total.read, value.read, "Read payload");
    accumulate(context, total.write, value.write, "Write payload");
}

void finish_payload(AnalysisContext& context,
                    PayloadFootprint& payload,
                    std::uint64_t multiplicity) {
    if (multiplicity != 0) {
        payload.logical_read =
            multiply(context, payload.read, multiplicity, "Logical read payload");
        payload.logical_write =
            multiply(context, payload.write, multiplicity, "Logical write payload");
    }
}

void finish_regions(AnalysisContext& context, RegionFootprint& footprint, Quantity payload) {
    if (footprint.region_bytes) {
        footprint.bytes =
            multiply(context, footprint.count, *footprint.region_bytes, "Region footprint bytes");
        footprint.read_bytes = multiply(
            context, footprint.read_count, *footprint.region_bytes, "Read region footprint bytes");
        footprint.write_bytes = multiply(context,
                                         footprint.write_count,
                                         *footprint.region_bytes,
                                         "Write region footprint bytes");
    }
    if (footprint.read_count == 0) {
        footprint.read_bytes = 0;
    }
    if (footprint.write_count == 0) {
        footprint.write_bytes = 0;
    }
    footprint.non_payload_bytes = difference(footprint.bytes, payload);
}

auto interval_regions(AnalysisContext& context,
                      Quantity stride,
                      Intervals& intervals,
                      std::uint64_t count,
                      Quantity region_size,
                      bool complete) -> Quantity {
    if (!complete) {
        return std::nullopt;
    }
    if (intervals.empty() || count == 0) {
        return 0;
    }
    if (!stride || !region_size) {
        return std::nullopt;
    }
    std::ranges::sort(intervals);
    std::string reason;
    auto const regions{touched_regions(*stride, intervals, count, *region_size, reason)};
    if (!regions) {
        error(context, "Access region coverage: " + reason);
    }
    return regions;
}

void interval_footprint(AnalysisContext& context,
                        RegionFootprint& result,
                        Quantity stride,
                        Intervals& all,
                        Intervals& reads,
                        Intervals& writes,
                        std::uint64_t count,
                        bool all_complete,
                        bool reads_complete,
                        bool writes_complete) {
    result.count = interval_regions(context, stride, all, count, result.region_bytes, all_complete);
    result.read_count =
        interval_regions(context, stride, reads, count, result.region_bytes, reads_complete);
    result.write_count =
        interval_regions(context, stride, writes, count, result.region_bytes, writes_complete);
}

void column_footprint(AnalysisContext& context,
                      RegionFootprint& result,
                      Quantity region_bytes,
                      Quantity element_bytes,
                      Quantity useful,
                      Quantity offset,
                      std::uint64_t count,
                      AccessOperation operation) {
    result.region_bytes = region_bytes;
    if (region_bytes && useful) {
        result.count = minimum_regions(*useful, *region_bytes);
        if (element_bytes) {
            assert(*element_bytes != 0);
            result.straddling_elements =
                offset ? straddling_elements_at_offset(
                             *element_bytes, count, *region_bytes, useful, *offset)
                       : straddling_elements(*element_bytes, count, *region_bytes, useful);
        }
    }
    result.read_count = access_includes_read(operation) ? result.count : Quantity{0};
    result.write_count = access_includes_write(operation) ? result.count : Quantity{0};
    finish_regions(context, result, useful);
}

auto find_child(std::span<std::string_view const> names, RowRange range, std::string_view name)
    -> std::optional<std::size_t> {
    auto const end{range.begin + range.count};
    for (auto i{range.begin}; i != end; ++i) {
        if (names[i] == name) {
            return i;
        }
    }
    return std::nullopt;
}

} // namespace workload_detail

auto analyze_access(AnalysisContext& context, AnalysisRequest const& request, AnalysisId analysis)
    -> WorkloadId {
    using namespace workload_detail;
    auto& storage{context.storage};
    auto& table{storage.workloads};
    auto& selections{storage.selections};
    WorkloadId const id{
        table.append(analysis, {selections.num(), 0}, request.element_count, request.multiplicity)};
    auto const row{id.value};
    auto const& packed{storage.types.packed()[analysis.value]};
    auto const& aggregate{storage.types.aggregate()[analysis.value]};
    auto const& soa{storage.types.soa()[analysis.value]};
    if (!packed && !aggregate && !soa) {
        context.report({DiagnosticSeverity::warning,
                        "Access analysis requires a physical layout projection."});
        return id;
    }
    assert(request.abi != nullptr);
    auto const& memory{request.abi->memory_facts()};
    auto const cache_size{memory.cache_line_bytes && *memory.cache_line_bytes != 0
                              ? memory.cache_line_bytes
                              : Quantity{}};
    auto const page_size{memory.page_bytes && *memory.page_bytes != 0 ? memory.page_bytes
                                                                      : Quantity{}};
    table.cache()[row].region_bytes = cache_size;
    table.pages()[row].region_bytes = page_size;
    table.cache_capacity()[row] = cache_capacity_analysis(std::nullopt, memory);
    if (request.multiplicity == 0) {
        error(context, "Access multiplicity must be non-zero.");
    }

    auto const range{packed ? packed->fields : soa ? soa->columns : aggregate->members};
    auto const names{packed ? storage.fields.name()
                     : soa  ? storage.columns.name()
                            : storage.components.name()};
    auto& payload{packed ? table.bits()[row] : table.bytes()[row]};
    payload.useful = 0;
    payload.read = 0;
    payload.write = 0;
    Intervals all;
    Intervals reads;
    Intervals writes;
    bool all_complete{true};
    bool reads_complete{true};
    bool writes_complete{true};
    bool has_reads{};
    bool has_writes{};
    std::uint64_t selected_bits{};
    std::uint64_t read_bits{};
    std::uint64_t written_bits{};
    auto const packed_mask = [&](std::size_t field) {
        auto const offset{storage.fields.least_significant_bit()[field]};
        auto const bits{storage.fields.bit_width()[field]};
        if (offset >= 64 || bits > 64 - offset) {
            return std::uint64_t{};
        }
        return bits == 64 ? ~std::uint64_t{} : ((std::uint64_t{1} << bits) - 1) << offset;
    };

    if (soa) {
        table.storage_bytes()[row] = soa->total_allocation_bytes;
        table.allocated_payload_bytes()[row] = soa->total_payload_bytes;
        table.full_logical_payload_bytes()[row] = multiply(
            context, soa->bytes_per_logical_element, request.element_count, "SoA logical payload");
        table.capacity_slack_bytes()[row] =
            difference(soa->total_payload_bytes, table.full_logical_payload_bytes()[row]);
        table.cache()[row].count = cache_size ? Quantity{0} : Quantity{};
        table.cache()[row].read_count = table.cache()[row].count;
        table.cache()[row].write_count = table.cache()[row].count;
        table.pages()[row].count = page_size ? Quantity{0} : Quantity{};
        table.pages()[row].read_count = table.pages()[row].count;
        table.pages()[row].write_count = table.pages()[row].count;
        if (request.element_count > soa->capacity) {
            context.report({DiagnosticSeverity::warning,
                            "Selected access count exceeds the modeled SoA allocation capacity."});
        }
    } else {
        auto const physical_size{packed ? packed->storage_facts.transform([&](PhysicalFactId fact) {
            return storage.physical_facts.size_bytes()[fact.value];
        })
                                        : aggregate->size_bytes};
        table.storage_bytes()[row] =
            multiply(context, physical_size, request.element_count, "Access storage bytes");
    }

    if (request.accesses != nullptr) {
        for (auto const& [name, operation] : *request.accesses) {
            auto const source{find_child(names, range, name)};
            if (!source) {
                error(context, "Selected member '" + name + "' no longer exists.");
                continue;
            }
            auto const child{*source};
            if (packed && storage.fields.reserved()[child]) {
                error(context,
                      "Reserved packed region '" + name + "' cannot be selected for access.");
                continue;
            }
            auto const selected{selections.append(child, operation)};
            ++table.selections()[row].count;
            has_reads |= access_includes_read(operation);
            has_writes |= access_includes_write(operation);
            auto const per_element{
                packed ? Quantity{storage.fields.bit_width()[child]}
                : soa  ? storage.columns.type_facts()[child].transform([&](PhysicalFactId fact) {
                      return storage.physical_facts.size_bytes()[fact.value];
                  })
                       : storage.components.extent_bytes()[child]};
            auto& selected_payload{packed ? selections.bits()[selected]
                                          : selections.bytes()[selected]};
            auto const useful{multiply(context,
                                       per_element,
                                       request.element_count,
                                       packed ? "Selected field bits" : "Selected payload bytes")};
            classify(context, selected_payload, useful, operation, request.multiplicity);
            accumulate_payload(context, payload, selected_payload);

            if (packed) {
                auto const mask{packed_mask(child)};
                selected_bits |= mask;
                if (access_includes_read(operation)) {
                    read_bits |= mask;
                }
                if (access_includes_write(operation)) {
                    written_bits |= mask;
                }
                continue;
            }
            auto const offset{soa ? storage.columns.allocation_offset_bytes()[child]
                                  : storage.components.offset_bytes()[child]};
            auto const extent{soa ? useful : per_element};
            if (offset && extent) {
                if (*extent != 0) {
                    all.emplace_back(*offset, *extent);
                    if (access_includes_read(operation)) {
                        reads.emplace_back(*offset, *extent);
                    }
                    if (access_includes_write(operation)) {
                        writes.emplace_back(*offset, *extent);
                    }
                }
            } else {
                all_complete = false;
                reads_complete &= !access_includes_read(operation);
                writes_complete &= !access_includes_write(operation);
            }
            if (!soa) {
                continue;
            }

            selections.capacity_slack_bytes()[selected] =
                difference(storage.columns.total_bytes()[child], useful);
            column_footprint(context,
                             selections.cache()[selected],
                             cache_size,
                             per_element,
                             useful,
                             offset,
                             request.element_count,
                             operation);
            column_footprint(context,
                             selections.pages()[selected],
                             page_size,
                             per_element,
                             useful,
                             offset,
                             request.element_count,
                             operation);
            auto add_regions = [&](RegionFootprint& total, RegionFootprint const& value) {
                accumulate(context, total.count, value.count, "Selected region count");
                accumulate(context, total.read_count, value.read_count, "Read region count");
                accumulate(context, total.write_count, value.write_count, "Write region count");
            };
            add_regions(table.cache()[row], selections.cache()[selected]);
            add_regions(table.pages()[row], selections.pages()[selected]);
        }
    }

    if (table.selections()[row].count == 0) {
        payload = {};
        table.cache()[row] = {.region_bytes = cache_size};
        table.pages()[row] = {.region_bytes = page_size};
        context.report({DiagnosticSeverity::warning, "No members are selected for access."});
        return id;
    }

    if (packed) {
        payload.useful = multiply(
            context, std::popcount(selected_bits), request.element_count, "Selected field bits");
        payload.read =
            multiply(context, std::popcount(read_bits), request.element_count, "Read field bits");
        payload.write = multiply(
            context, std::popcount(written_bits), request.element_count, "Written field bits");
    }
    finish_payload(context, payload, request.multiplicity);
    if (packed) {
        auto& waste{table.packed_waste()[row]};
        waste.storage_bits =
            multiply(context, table.storage_bytes()[row], 8, "Packed storage bits");
        waste.non_useful_bits = difference(waste.storage_bits, payload.useful);
        if (waste.storage_bits && payload.useful && !waste.non_useful_bits) {
            error(context, "Selected useful bits exceed the packed physical storage footprint.");
        }
        if (packed->unused_bits && waste.storage_bits) {
            std::uint64_t unselected{};
            auto const end{range.begin + range.count};
            for (auto field{range.begin}; field != end; ++field) {
                if (!storage.fields.reserved()[field] &&
                    !request.accesses->contains(storage.fields.name()[field])) {
                    unselected |= packed_mask(field);
                }
            }
            waste.unselected_field_bits = multiply(context,
                                                   std::popcount(unselected & ~selected_bits),
                                                   request.element_count,
                                                   "Unselected field bits");
            waste.reserved_bits = multiply(
                context, packed->reserved_bits, request.element_count, "Reserved region bits");
            waste.unused_bits = multiply(
                context, packed->unused_bits, request.element_count, "Unused storage bits");
        }
        auto packed_regions = [&](RegionFootprint& footprint) {
            if (footprint.region_bytes && table.storage_bytes()[row]) {
                footprint.count =
                    minimum_regions(*table.storage_bytes()[row], *footprint.region_bytes);
            }
            footprint.read_count = has_reads ? footprint.count : Quantity{0};
            footprint.write_count = has_writes ? footprint.count : Quantity{0};
        };
        packed_regions(table.cache()[row]);
        packed_regions(table.pages()[row]);
    } else if (soa) {
        table.unselected_payload_bytes()[row] =
            difference(table.full_logical_payload_bytes()[row], payload.useful);
        auto const complete{request.element_count <= soa->capacity &&
                            soa->total_allocation_bytes.has_value()};
        table.footprint_exact()[row] = complete && all_complete;
        interval_footprint(context,
                           table.cache()[row],
                           0,
                           all,
                           reads,
                           writes,
                           1,
                           complete && all_complete,
                           complete && reads_complete,
                           complete && writes_complete);
        interval_footprint(context,
                           table.pages()[row],
                           0,
                           all,
                           reads,
                           writes,
                           1,
                           complete && all_complete,
                           complete && reads_complete,
                           complete && writes_complete);
    } else {
        table.footprint_exact()[row] = all_complete && aggregate->size_bytes.has_value();
        interval_footprint(context,
                           table.cache()[row],
                           aggregate->size_bytes,
                           all,
                           reads,
                           writes,
                           request.element_count,
                           all_complete,
                           reads_complete,
                           writes_complete);
        interval_footprint(context,
                           table.pages()[row],
                           aggregate->size_bytes,
                           all,
                           reads,
                           writes,
                           request.element_count,
                           all_complete,
                           reads_complete,
                           writes_complete);
    }

    if (!has_reads) {
        table.cache()[row].read_count = 0;
        table.pages()[row].read_count = 0;
    }
    if (!has_writes) {
        table.cache()[row].write_count = 0;
        table.pages()[row].write_count = 0;
    }
    finish_regions(context, table.cache()[row], packed ? Quantity{} : payload.useful);
    finish_regions(context, table.pages()[row], packed ? Quantity{} : payload.useful);
    table.cache_capacity()[row] = cache_capacity_analysis(table.cache()[row].bytes, memory);
    return id;
}

} // namespace ioj::layout
