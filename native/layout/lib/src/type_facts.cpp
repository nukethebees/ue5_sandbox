#include <ioj/layout/type_facts.hpp>

#include <bit>

namespace ioj::layout {

auto validate_type_facts(TypeFacts const& facts) -> std::expected<void, std::string> {
    if (facts.size_bytes == 0 || !std::has_single_bit(facts.alignment_bytes)) {
        return std::unexpected{
            "Complete-object size must be nonzero and alignment a nonzero power of two."};
    }
    if (facts.size_bytes % facts.alignment_bytes != 0) {
        return std::unexpected{
            "Complete-object size must be a multiple of alignment (include tail padding)."};
    }
    if (facts.unsigned_value_bits.has_value()) {
        auto const bits{*facts.unsigned_value_bits};
        if (facts.integer_signed != false || bits == 0 ||
            facts.size_bytes < (static_cast<std::uint64_t>(bits) + 7) / 8) {
            return std::unexpected{
                "Unsigned value bits require an unsigned integer and must fit its physical width."};
        }
    }
    return {};
}

} // namespace ioj::layout
