#include <ioj/layout/abi_profile.hpp>

#include <cstdint>
#include <limits>
#include <set>
#include <type_traits>
#include <utility>

namespace ioj::layout {
namespace {

template <typename T>
auto integer_facts() -> TypeFacts {
    TypeFacts facts{.size_bytes = sizeof(T),
                    .alignment_bytes = alignof(T),
                    .integer_signed = std::is_signed_v<T>,
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
            .integer_signed = std::nullopt,
            .unsigned_value_bits = std::nullopt};
}

} // namespace

AbiProfile::AbiProfile(std::string name)
    : name_{std::move(name)} {}

auto AbiProfile::host_common() -> AbiProfile {
    AbiProfile result{"Host common native"};
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
    result.set_memory_facts({.cache_line_bytes = 64,
                             .page_bytes = 4'096,
                             .provenance = "Explicit x86/x86-64 baseline profile"});
#else
    result.set_memory_facts({.cache_line_bytes = std::nullopt,
                             .page_bytes = std::nullopt,
                             .provenance = "Unspecified host architecture"});
#endif
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
                         .integer_signed = false,
                         .unsigned_value_bits = 1});
    return result;
}

void AbiProfile::set(std::string spelling, TypeFacts facts) {
    types_.insert_or_assign(std::move(spelling), facts);
}

void AbiProfile::set_representation(std::string spelling, std::string represented_by) {
    representations_.insert_or_assign(std::move(spelling), std::move(represented_by));
}

void AbiProfile::set_memory_facts(MemoryFacts facts) {
    memory_facts_ = std::move(facts);
}

auto AbiProfile::find(std::string const& spelling) const -> std::optional<TypeFacts> {
    std::set<std::string, std::less<>> visited;
    auto current{spelling};
    while (visited.insert(current).second) {
        if (auto const found{types_.find(current)}; found != types_.end()) {
            return found->second;
        }
        auto const representation{representations_.find(current)};
        if (representation == representations_.end()) {
            return std::nullopt;
        }
        current = representation->second;
    }
    return std::nullopt;
}

auto AbiProfile::name() const -> std::string const& {
    return name_;
}

auto AbiProfile::types() const -> std::map<std::string, TypeFacts, std::less<>> const& {
    return types_;
}

auto AbiProfile::memory_facts() const -> MemoryFacts const& {
    return memory_facts_;
}

} // namespace ioj::layout
