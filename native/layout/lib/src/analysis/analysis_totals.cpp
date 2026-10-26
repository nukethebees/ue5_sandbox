#include <ioj/layout/analysis/analysis_totals.hpp>

#include "../checked_arithmetic.hpp"
#include "../memory_regions.hpp"
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analysis/analysis_context.hpp>
#include <ioj/layout/analysis/analysis_request.hpp>
#include <ioj/layout/analysis/analysis_storage.hpp>

#include <lispb/schema/type_graph.h>

#include <cassert>
#include <string>

namespace ioj::layout {

auto compute_element_totals(AnalysisContext& context,
                            AnalysisRequest const& request,
                            AnalysisId analysis) -> ElementTotals {
    using namespace analysis_detail;
    using Quantity = std::optional<std::uint64_t>;
    auto const& storage{context.storage};
    auto const& physical{storage.types.physical()[analysis.value]};
    auto const& aggregate{storage.types.aggregate()[analysis.value]};
    auto const& packed{storage.types.packed()[analysis.value]};
    auto const& soa{storage.types.soa()[analysis.value]};
    ElementTotals result{.element_count = request.element_count};

    auto scale = [&](Quantity value, std::string_view label) -> Quantity {
        if (!value) {
            return std::nullopt;
        }
        auto const total{checked_multiply(*value, request.element_count)};
        if (!total) {
            context.report(
                {DiagnosticSeverity::error, std::string{label} + " total overflows uint64."});
        }
        return total;
    };

    auto const element_bytes{physical.transform(
        [&](PhysicalFactId fact) { return storage.physical_facts.size_bytes()[fact.value]; })};
    if (!soa) {
        result.total_storage_bytes = scale(element_bytes, "Storage bytes");
    }
    if (aggregate) {
        result.total_payload_bytes = scale(aggregate->payload_bytes, "Payload bytes");
        result.total_internal_padding_bytes =
            scale(aggregate->internal_padding_bytes, "Internal padding bytes");
        result.total_tail_padding_bytes =
            scale(aggregate->tail_padding_bytes, "Tail padding bytes");
        if (result.total_internal_padding_bytes && result.total_tail_padding_bytes) {
            result.total_padding_bytes =
                checked_add(*result.total_internal_padding_bytes, *result.total_tail_padding_bytes);
            if (!result.total_padding_bytes) {
                context.report({DiagnosticSeverity::error, "Padding byte total overflows uint64."});
            }
        }
    }
    if (packed) {
        result.total_payload_bits = scale(packed->payload_bits, "Packed payload bits");
        result.total_reserved_bits = scale(packed->reserved_bits, "Packed reserved bits");
        result.total_unused_bits = scale(packed->unused_bits, "Packed unused bits");
    }
    if (soa) {
        result.total_storage_bytes = soa->total_allocation_bytes;
        result.total_payload_bytes = soa->total_payload_bytes;
        result.total_padding_bytes = soa->total_alignment_padding_bytes;
    }

    auto const kind{context.types.definition(request.type).kind()};
    auto const& scalars{storage.scalars};
    using Kind = lispb::schema::TypeDefinitionKind;
    auto encoding = [&](auto const column) -> auto const& {
        return *column[scalars.find(request.type)];
    };
    switch (kind) {
        case Kind::optional_sentinel:
            result.total_encoded_bits =
                scale(encoding(scalars.optional_sentinel_encoding()).encoded_storage_bits,
                      "Optional encoding bits");
            break;
        case Kind::optional_presence_bit:
            result.total_encoded_bits =
                scale(encoding(scalars.optional_presence_encoding()).encoded_storage_bits,
                      "Optional encoding bits");
            break;
        case Kind::linear_quantized:
            result.total_encoded_bits =
                scale(encoding(scalars.quantized_encoding()).encoded_storage_bits,
                      "Quantized encoding bits");
            break;
        case Kind::fixed_point:
            result.total_encoded_bits =
                scale(encoding(scalars.fixed_encoding()).total_bits, "Fixed-point encoding bits");
            break;
        case Kind::mini_float:
            result.total_encoded_bits =
                scale(encoding(scalars.float_encoding()).total_bits, "Mini-float encoding bits");
            break;
        case Kind::integer_varint: {
            auto const& varint{encoding(scalars.varint_encoding())};
            result.minimum_total_bytes =
                scale(varint.minimum_encoded_bytes, "Minimum encoded bytes");
            result.maximum_total_bytes =
                scale(varint.maximum_encoded_bytes, "Maximum encoded bytes");
            break;
        }
        default:
            break;
    }

    if (request.abi == nullptr) {
        return result;
    }
    auto const& memory{request.abi->memory_facts()};
    result.cache_capacity = cache_capacity_analysis(result.total_storage_bytes, memory);
    auto regions = [&](Quantity region_size,
                       Quantity& minimum_count,
                       Quantity& complete_elements,
                       Quantity& straddling) {
        if (!region_size || *region_size == 0) {
            return;
        }
        if (result.total_storage_bytes) {
            minimum_count = minimum_regions(*result.total_storage_bytes, *region_size);
        }
        if (!soa && element_bytes) {
            assert(*element_bytes != 0);
            complete_elements = *region_size / *element_bytes;
            straddling = straddling_elements(
                *element_bytes, request.element_count, *region_size, result.total_storage_bytes);
        }
    };
    regions(memory.cache_line_bytes,
            result.minimum_cache_lines,
            result.complete_elements_per_cache_line,
            result.cache_line_straddling_elements);
    regions(memory.page_bytes,
            result.minimum_pages,
            result.complete_elements_per_page,
            result.page_straddling_elements);
    return result;
}

} // namespace ioj::layout
