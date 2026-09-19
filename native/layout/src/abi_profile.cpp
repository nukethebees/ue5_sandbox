#include <ioj/layout/abi_profile.hpp>

#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace ioj::layout {
namespace {

template <typename T>
auto integer_facts() -> TypeFacts {
    TypeFacts facts{.size_bytes = sizeof(T),
                    .alignment_bytes = alignof(T),
                    .unsigned_value_bits = std::nullopt};
    if constexpr (std::is_unsigned_v<T>) {
        facts.unsigned_value_bits = std::numeric_limits<T>::digits;
    }
    return facts;
}

template <typename T>
auto value_facts() -> TypeFacts {
    return {.size_bytes = sizeof(T),
            .alignment_bytes = alignof(T),
            .unsigned_value_bits = std::nullopt};
}

} // namespace

AbiProfile::AbiProfile(std::string name)
    : name_{std::move(name)} {}

auto AbiProfile::host_common() -> AbiProfile {
    AbiProfile result{"Host common native"};
    result.set("std::uint8_t", integer_facts<std::uint8_t>());
    result.set("std::uint16_t", integer_facts<std::uint16_t>());
    result.set("std::uint32_t", integer_facts<std::uint32_t>());
    result.set("std::uint64_t", integer_facts<std::uint64_t>());
    result.set("std::int8_t", integer_facts<std::int8_t>());
    result.set("std::int16_t", integer_facts<std::int16_t>());
    result.set("std::int32_t", integer_facts<std::int32_t>());
    result.set("std::int64_t", integer_facts<std::int64_t>());
    result.set("float", value_facts<float>());
    result.set("double", value_facts<double>());
    result.set("bool",
               TypeFacts{.size_bytes = sizeof(bool),
                         .alignment_bytes = alignof(bool),
                         .unsigned_value_bits = 1});
    return result;
}

void AbiProfile::set(std::string spelling, TypeFacts facts) {
    types_.insert_or_assign(std::move(spelling), facts);
}

auto AbiProfile::find(std::string const& spelling) const -> std::optional<TypeFacts> {
    auto const found{types_.find(spelling)};
    if (found == types_.end()) {
        return std::nullopt;
    }
    return found->second;
}

auto AbiProfile::name() const -> std::string const& {
    return name_;
}

auto AbiProfile::types() const -> std::map<std::string, TypeFacts, std::less<>> const& {
    return types_;
}

} // namespace ioj::layout
