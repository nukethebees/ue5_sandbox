#include <ioj/layout/abi_profile.hpp>

#include <charconv>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ioj::layout {
namespace {

inline constexpr char compiler_fact_provenance[]{"Compiler-derived sizeof/alignof for this build"};

template <typename T>
auto integer_facts() -> TypeFacts {
    TypeFacts facts{.size_bytes = sizeof(T),
                    .alignment_bytes = alignof(T),
                    .integer_signed = std::is_signed_v<T>,
                    .unsigned_value_bits = std::nullopt,
                    .provenance = compiler_fact_provenance};
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
            .unsigned_value_bits = std::nullopt,
            .provenance = compiler_fact_provenance};
}

auto parse_error(std::size_t const line, std::string message)
    -> std::unexpected<AbiProfileParseError> {
    return std::unexpected{AbiProfileParseError{.line = line, .message = std::move(message)}};
}

auto read_quoted(std::istringstream& input, std::string& value) -> bool {
    input >> std::ws;
    if (input.peek() != '"') {
        return false;
    }
    return static_cast<bool>(input >> std::quoted(value));
}

auto read_token(std::istringstream& input, std::string& value) -> bool {
    return static_cast<bool>(input >> value);
}

auto parse_unsigned(std::string_view const token) -> std::optional<std::uint64_t> {
    std::uint64_t result{};
    auto const [end, error]{std::from_chars(token.data(), token.data() + token.size(), result)};
    if (error != std::errc{} || end != token.data() + token.size()) {
        return std::nullopt;
    }
    return result;
}

auto has_trailing_input(std::istringstream& input) -> bool {
    input >> std::ws;
    return !input.eof();
}

auto is_power_of_two(std::uint64_t const value) -> bool {
    return value != 0 && (value & (value - 1)) == 0;
}

auto profile_has_representation_cycle(
    std::map<std::string, std::string, std::less<>> const& representations) -> bool {
    for (auto const& [source, target] : representations) {
        static_cast<void>(target);
        std::set<std::string, std::less<>> visited;
        auto current{source};
        auto found{representations.find(current)};
        while (found != representations.end()) {
            if (!visited.insert(current).second) {
                return true;
            }
            current = found->second;
            found = representations.find(current);
        }
    }
    return false;
}

void write_identity(std::ostringstream& output,
                    char const* const name,
                    std::optional<std::string> const& value) {
    if (value.has_value()) {
        output << "identity " << name << ' ' << std::quoted(*value) << '\n';
    }
}

void write_memory_fact(std::ostringstream& output,
                       char const* const name,
                       std::optional<std::uint64_t> const value) {
    if (value.has_value()) {
        output << "memory " << name << ' ' << *value << '\n';
    }
}

} // namespace

AbiProfile::AbiProfile(std::string name, AbiProfileIdentity identity)
    : name_{std::move(name)}
    , identity_{std::move(identity)} {}

auto AbiProfile::host_common() -> AbiProfile {
    AbiProfile result{"Host compiler profile",
                      {.platform = IOJ_LAYOUT_TARGET_PLATFORM,
                       .architecture = IOJ_LAYOUT_TARGET_ARCHITECTURE,
                       .abi = std::nullopt,
                       .compiler = IOJ_LAYOUT_TARGET_COMPILER,
                       .build_configuration = IOJ_LAYOUT_BUILD_CONFIGURATION}};
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
    result.set_memory_facts({.cache_line_bytes = 64,
                             .page_bytes = 4'096,
                             .l1_data_cache_bytes = std::nullopt,
                             .l2_cache_bytes = std::nullopt,
                             .l3_cache_bytes = std::nullopt,
                             .provenance = "Explicit x86/x86-64 baseline profile"});
#else
    result.set_memory_facts({.cache_line_bytes = std::nullopt,
                             .page_bytes = std::nullopt,
                             .l1_data_cache_bytes = std::nullopt,
                             .l2_cache_bytes = std::nullopt,
                             .l3_cache_bytes = std::nullopt,
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
                         .unsigned_value_bits = 1,
                         .provenance = compiler_fact_provenance});
    return result;
}

void AbiProfile::set(std::string spelling, TypeFacts const& facts) {
    types_.insert_or_assign(std::move(spelling), facts);
}

void AbiProfile::set_representation(std::string spelling, std::string represented_by) {
    representations_.insert_or_assign(std::move(spelling), std::move(represented_by));
}

void AbiProfile::set_memory_facts(MemoryFacts facts) {
    memory_facts_ = std::move(facts);
}

void AbiProfile::set_identity(AbiProfileIdentity identity) {
    identity_ = std::move(identity);
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

auto AbiProfile::identity() const -> AbiProfileIdentity const& {
    return identity_;
}

auto AbiProfile::types() const -> std::map<std::string, TypeFacts, std::less<>> const& {
    return types_;
}

auto AbiProfile::representations() const -> std::map<std::string, std::string, std::less<>> const& {
    return representations_;
}

auto AbiProfile::memory_facts() const -> MemoryFacts const& {
    return memory_facts_;
}

auto parse_abi_profile(std::string_view const source)
    -> std::expected<AbiProfile, AbiProfileParseError> {
    std::istringstream source_stream{std::string{source}};
    std::string line;
    std::size_t line_number{};
    bool saw_header{};
    bool saw_name{};
    bool saw_memory_provenance{};
    std::string profile_name;
    AbiProfileIdentity identity;
    std::set<std::string, std::less<>> identity_fields;
    std::map<std::string, TypeFacts, std::less<>> types;
    std::map<std::string, std::string, std::less<>> representations;
    std::set<std::string, std::less<>> memory_fields;
    MemoryFacts memory;

    while (std::getline(source_stream, line)) {
        ++line_number;
        std::istringstream input{line};
        input >> std::ws;
        if (input.eof() || input.peek() == '#') {
            continue;
        }

        std::string directive;
        input >> directive;
        if (!saw_header) {
            std::string version;
            if (directive != "ioj-layout-profile" || !read_token(input, version) ||
                version != "1" || has_trailing_input(input)) {
                return parse_error(line_number, "Expected 'ioj-layout-profile 1' header.");
            }
            saw_header = true;
            continue;
        }

        if (directive == "name") {
            if (saw_name) {
                return parse_error(line_number, "Duplicate profile name.");
            }
            if (!read_quoted(input, profile_name) || profile_name.empty() ||
                has_trailing_input(input)) {
                return parse_error(line_number, "Profile name must be one nonempty quoted string.");
            }
            saw_name = true;
            continue;
        }

        if (directive == "identity") {
            std::string field;
            std::string value;
            if (!read_token(input, field) || !read_quoted(input, value) || value.empty() ||
                has_trailing_input(input)) {
                return parse_error(line_number,
                                   "Identity requires a field and one nonempty quoted value.");
            }
            if (!identity_fields.insert(field).second) {
                return parse_error(line_number, "Duplicate identity field '" + field + "'.");
            }
            auto* destination{&identity.platform};
            if (field == "architecture") {
                destination = &identity.architecture;
            } else if (field == "abi") {
                destination = &identity.abi;
            } else if (field == "compiler") {
                destination = &identity.compiler;
            } else if (field == "build-configuration") {
                destination = &identity.build_configuration;
            } else if (field != "platform") {
                return parse_error(line_number, "Unknown identity field '" + field + "'.");
            }
            *destination = std::move(value);
            continue;
        }

        if (directive == "type") {
            std::string spelling;
            std::string size_token;
            std::string alignment_token;
            std::string kind;
            std::string bits_token;
            std::string provenance;
            if (!read_quoted(input, spelling) || !read_token(input, size_token) ||
                !read_token(input, alignment_token) || !read_token(input, kind) ||
                !read_token(input, bits_token) || !read_quoted(input, provenance) ||
                has_trailing_input(input)) {
                return parse_error(line_number,
                                   "Type requires quoted spelling, size, alignment, integer kind, "
                                   "value bits, and quoted provenance.");
            }
            auto const size{parse_unsigned(size_token)};
            auto const alignment{parse_unsigned(alignment_token)};
            if (spelling.empty() || !size.has_value() || *size == 0 || !alignment.has_value() ||
                !is_power_of_two(*alignment)) {
                return parse_error(line_number,
                                   "Type spelling must be nonempty, size nonzero, and alignment a "
                                   "nonzero power of two.");
            }
            if (types.contains(spelling) || representations.contains(spelling)) {
                return parse_error(line_number,
                                   "Duplicate type or representation '" + spelling + "'.");
            }

            TypeFacts facts{.size_bytes = *size,
                            .alignment_bytes = *alignment,
                            .integer_signed = std::nullopt,
                            .unsigned_value_bits = std::nullopt,
                            .provenance = std::move(provenance)};
            if (kind == "signed") {
                facts.integer_signed = true;
                if (bits_token != "unknown") {
                    return parse_error(
                        line_number,
                        "Signed integer facts must use 'unknown' unsigned value bits.");
                }
            } else if (kind == "unsigned") {
                facts.integer_signed = false;
                auto const bits{parse_unsigned(bits_token)};
                if (!bits.has_value() || *bits == 0 ||
                    *bits > std::numeric_limits<std::uint32_t>::max() || *size < (*bits + 7) / 8) {
                    return parse_error(
                        line_number, "Unsigned value bits must be within the physical type width.");
                }
                facts.unsigned_value_bits = static_cast<std::uint32_t>(*bits);
            } else if (kind == "non-integer") {
                if (bits_token != "unknown") {
                    return parse_error(line_number,
                                       "Non-integer facts must use 'unknown' unsigned value bits.");
                }
            } else {
                return parse_error(line_number, "Unknown integer kind '" + kind + "'.");
            }
            types.emplace(std::move(spelling), std::move(facts));
            continue;
        }

        if (directive == "representation") {
            std::string spelling;
            std::string target;
            if (!read_quoted(input, spelling) || !read_quoted(input, target) || spelling.empty() ||
                target.empty() || has_trailing_input(input)) {
                return parse_error(line_number,
                                   "Representation requires two nonempty quoted type spellings.");
            }
            if (types.contains(spelling) || representations.contains(spelling)) {
                return parse_error(line_number,
                                   "Duplicate type or representation '" + spelling + "'.");
            }
            representations.emplace(std::move(spelling), std::move(target));
            continue;
        }

        if (directive == "memory") {
            std::string field;
            std::string value_token;
            if (!read_token(input, field) || !read_token(input, value_token) ||
                has_trailing_input(input)) {
                return parse_error(line_number, "Memory requires a field and unsigned byte value.");
            }
            auto const value{parse_unsigned(value_token)};
            if (!value.has_value()) {
                return parse_error(line_number, "Memory byte values must be unsigned integers.");
            }
            auto* destination{&memory.cache_line_bytes};
            auto require_non_zero{true};
            if (field == "page") {
                destination = &memory.page_bytes;
            } else if (field == "l1-data") {
                destination = &memory.l1_data_cache_bytes;
                require_non_zero = false;
            } else if (field == "l2") {
                destination = &memory.l2_cache_bytes;
                require_non_zero = false;
            } else if (field == "l3") {
                destination = &memory.l3_cache_bytes;
                require_non_zero = false;
            } else if (field != "cache-line") {
                return parse_error(line_number, "Unknown memory field '" + field + "'.");
            }
            if (require_non_zero && *value == 0) {
                return parse_error(line_number, "Cache-line and page byte values must be nonzero.");
            }
            if (!memory_fields.insert(field).second) {
                return parse_error(line_number, "Duplicate memory field '" + field + "'.");
            }
            *destination = *value;
            continue;
        }

        if (directive == "memory-provenance") {
            if (saw_memory_provenance || !read_quoted(input, memory.provenance) ||
                has_trailing_input(input)) {
                return parse_error(line_number,
                                   "Memory provenance must be one unique quoted string.");
            }
            saw_memory_provenance = true;
            continue;
        }

        return parse_error(line_number, "Unknown target-profile directive '" + directive + "'.");
    }

    if (!saw_header) {
        return parse_error(0, "Missing 'ioj-layout-profile 1' header.");
    }
    if (!saw_name) {
        return parse_error(0, "Missing profile name.");
    }
    if (profile_has_representation_cycle(representations)) {
        return parse_error(0, "Target profile contains a representation cycle.");
    }

    AbiProfile result{std::move(profile_name), std::move(identity)};
    for (auto const& [spelling, facts] : types) {
        result.set(spelling, facts);
    }
    for (auto const& [spelling, represented_by] : representations) {
        result.set_representation(spelling, represented_by);
    }
    result.set_memory_facts(std::move(memory));
    return result;
}

auto load_abi_profile(std::filesystem::path const& path)
    -> std::expected<AbiProfile, AbiProfileParseError> {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return parse_error(0, "Unable to open target profile '" + path.string() + "'.");
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.eof() && input.fail()) {
        return parse_error(0, "Unable to read target profile '" + path.string() + "'.");
    }
    return parse_abi_profile(contents.str());
}

auto serialize_abi_profile(AbiProfile const& profile) -> std::string {
    std::ostringstream output;
    output << "ioj-layout-profile 1\n";
    output << "name " << std::quoted(profile.name()) << '\n';
    auto const& identity{profile.identity()};
    write_identity(output, "platform", identity.platform);
    write_identity(output, "architecture", identity.architecture);
    write_identity(output, "abi", identity.abi);
    write_identity(output, "compiler", identity.compiler);
    write_identity(output, "build-configuration", identity.build_configuration);
    for (auto const& [spelling, facts] : profile.types()) {
        auto const kind{!facts.integer_signed.has_value()
                            ? "non-integer"
                            : (*facts.integer_signed ? "signed" : "unsigned")};
        output << "type " << std::quoted(spelling) << ' ' << facts.size_bytes << ' '
               << facts.alignment_bytes << ' ' << kind << ' ';
        if (facts.unsigned_value_bits.has_value()) {
            output << *facts.unsigned_value_bits;
        } else {
            output << "unknown";
        }
        output << ' ' << std::quoted(facts.provenance) << '\n';
    }
    for (auto const& [spelling, represented_by] : profile.representations()) {
        output << "representation " << std::quoted(spelling) << ' ' << std::quoted(represented_by)
               << '\n';
    }
    auto const& memory{profile.memory_facts()};
    write_memory_fact(output, "cache-line", memory.cache_line_bytes);
    write_memory_fact(output, "page", memory.page_bytes);
    write_memory_fact(output, "l1-data", memory.l1_data_cache_bytes);
    write_memory_fact(output, "l2", memory.l2_cache_bytes);
    write_memory_fact(output, "l3", memory.l3_cache_bytes);
    output << "memory-provenance " << std::quoted(memory.provenance) << '\n';
    return output.str();
}

} // namespace ioj::layout
