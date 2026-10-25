#include <codegen/schema/mini_float_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto MiniFloatSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto MiniFloatSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    name.push_back({});
    sign_bits.push_back({});
    exponent_bits.push_back({});
    significand_bits.push_back({});
    exponent_bias.push_back({});
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::mini_float) << 24) |
                             (index + 1)};
}
auto MiniFloatSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    sign_bits[destination] = sign_bits[source_index];
    exponent_bits[destination] = exponent_bits[source_index];
    significand_bits[destination] = significand_bits[source_index];
    exponent_bias[destination] = exponent_bias[source_index];
    return result;
}
void MiniFloatSchemas::clear() {
    name.clear();
    sign_bits.clear();
    exponent_bits.clear();
    significand_bits.clear();
    exponent_bias.clear();
}

}
