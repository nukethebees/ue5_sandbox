#include <lispb/schema/enum_domains.h>

#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/module_kind.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <charconv>
#include <limits>
#include <string_view>
#include <system_error>
#include <utility>

namespace lispb::schema {
namespace {

auto parse_enum_code(std::string_view text) -> std::optional<EnumCode> {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    bool negative{};
    if (text.starts_with('+') || text.starts_with('-')) {
        negative = text.front() == '-';
        text.remove_prefix(1);
    }
    auto base{10};
    if (text.starts_with("0x") || text.starts_with("0X")) {
        base = 16;
        text.remove_prefix(2);
    }
    while (!text.empty() &&
           (text.back() == 'u' || text.back() == 'U' || text.back() == 'l' || text.back() == 'L')) {
        text.remove_suffix(1);
    }
    if (text.empty()) {
        return std::nullopt;
    }
    std::string digits;
    digits.reserve(text.size());
    for (auto const character : text) {
        if (character != '\'') {
            digits += character;
        }
    }
    std::uint64_t magnitude{};
    auto const [end, error]{
        std::from_chars(digits.data(), digits.data() + digits.size(), magnitude, base)};
    if (error != std::errc{} || end != digits.data() + digits.size()) {
        return std::nullopt;
    }
    return EnumCode{.negative = negative && magnitude != 0, .magnitude = magnitude};
}

auto next_enum_code(EnumCode const value) -> std::optional<EnumCode> {
    if (value.negative) {
        return value.magnitude == 1
                 ? std::optional{EnumCode{.negative = false, .magnitude = 0}}
                 : std::optional{EnumCode{.negative = true, .magnitude = value.magnitude - 1}};
    }
    if (value.magnitude == (std::numeric_limits<std::uint64_t>::max)()) {
        return std::nullopt;
    }
    return EnumCode{.negative = false, .magnitude = value.magnitude + 1};
}

auto enum_code_less(EnumCode const left, EnumCode const right) -> bool {
    if (left.negative != right.negative) {
        return left.negative;
    }
    return left.negative ? left.magnitude > right.magnitude : left.magnitude < right.magnitude;
}

auto minimum_enum_bits(EnumCode const minimum, EnumCode const maximum, bool const signed_domain)
    -> std::optional<std::uint8_t> {
    if (!signed_domain) {
        if (minimum.negative) {
            return std::nullopt;
        }
        auto bits{std::uint8_t{1}};
        auto remaining{maximum.magnitude};
        while (remaining > 1) {
            remaining >>= 1;
            ++bits;
        }
        return bits;
    }
    for (auto bits{std::uint8_t{1}}; bits <= 64; ++bits) {
        auto const magnitude_limit{bits == 64 ? (std::uint64_t{1} << 63)
                                              : (std::uint64_t{1} << (bits - 1))};
        auto const positive_limit{magnitude_limit - 1};
        if (minimum.magnitude <= magnitude_limit &&
            (maximum.negative || maximum.magnitude <= positive_limit)) {
            return bits;
        }
    }
    return std::nullopt;
}

} // namespace

EnumDomains::EnumDomains(codegen::Declarations const& declarations)
    : declarations_{&declarations}
    , declarations_to_domains_(declarations.enums.name.size()) {}

auto EnumDomains::index(EnumDomainId const id) const -> std::uint32_t {
    assert(id.value != 0 && id.value <= values_.size());
    return id.value - 1;
}
auto EnumDomains::find(codegen::DeclarationHandle const declaration) const -> EnumDomainId {
    auto const row{declarations_->enums.index(declaration)};
    auto const id{declarations_to_domains_[row]};
    assert(id.value != 0);
    return id;
}
auto EnumDomains::values(EnumDomainId const id) const -> std::span<codegen::EnumeratorId const> {
    return declarations_->items(values_[index(id)]);
}
auto EnumDomains::code(EnumDomainId const id, std::size_t const ordinal) const
    -> std::optional<EnumCode> {
    auto const row{index(id)};
    assert(ordinal < values_[row].size());
    return codes_[code_offsets_[row] + ordinal];
}

auto EnumDomains::contiguous_value_range(EnumDomainId const id) const
    -> std::optional<ioj::MinMax<std::uint64_t>> {
    auto const enumerators{values(id)};
    auto const count{count_value[index(id)]};
    std::vector<std::uint64_t> codes;
    codes.reserve(enumerators.size());
    auto const size{enumerators.size()};
    for (std::size_t ordinal{}; ordinal < size; ++ordinal) {
        if (enumerators[ordinal] == count) {
            continue;
        }
        auto const value{code(id, ordinal)};
        if (!value || value->negative) {
            return std::nullopt;
        }
        codes.push_back(value->magnitude);
    }
    if (codes.empty()) {
        return std::nullopt;
    }
    std::ranges::sort(codes);
    auto const last{std::unique(codes.begin(), codes.end())};
    auto const distinct{static_cast<std::uint64_t>(last - codes.begin())};
    auto const maximum{*(last - 1)};
    if (maximum - codes.front() != distinct - 1) {
        return std::nullopt;
    }
    return ioj::MinMax<std::uint64_t>{codes.front(), maximum};
}
auto EnumDomains::issue_count(EnumDomainId const id) const -> std::uint32_t {
    return issue_ranges_[index(id)].count;
}
auto EnumDomains::issue_severity(EnumDomainId const id, std::size_t const ordinal) const
    -> EnumDomainIssueSeverity {
    auto const range{issue_ranges_[index(id)]};
    assert(ordinal < range.count);
    switch (issue_kinds_[range.begin + ordinal]) {
        case EnumDomainIssueKind::non_literal:
        case EnumDomainIssueKind::implicit_unknown:
            return EnumDomainIssueSeverity::warning;
        default:
            return EnumDomainIssueSeverity::error;
    }
}
auto EnumDomains::issue_message(EnumDomainId const id, std::size_t const ordinal) const
    -> std::string {
    auto const row{index(id)};
    auto const range{issue_ranges_[row]};
    assert(ordinal < range.count);
    auto const issue{range.begin + ordinal};
    auto const& enumerators{declarations_->enumerators};
    auto const& strings{declarations_->strings()};
    auto const value{issue_values_[issue]};
    auto name = [&] {
        return std::string{strings.view(enumerators.name[enumerators.index(value)])};
    };
    switch (issue_kinds_[issue]) {
        case EnumDomainIssueKind::non_literal:
            return "Enumerator '" + name() + "' uses non-literal initializer '" +
                   std::string{strings.view(*enumerators.initializer[enumerators.index(value)])} +
                   "'; value-domain width is unknown.";
        case EnumDomainIssueKind::implicit_overflow:
            return "Implicit value for enumerator '" + name() +
                   "' overflows the supported 64-bit analysis range.";
        case EnumDomainIssueKind::implicit_unknown:
            return "Implicit value for enumerator '" + name() +
                   "' cannot be derived after an unknown initializer.";
        case EnumDomainIssueKind::negative_unsigned:
            return "Explicitly unsigned enum domain contains negative value " +
                   format_enum_code(*minimum_value[row]) + ".";
        case EnumDomainIssueKind::width_overflow:
            return "Enum value domain does not fit its signedness within the supported 64-bit "
                   "analysis range.";
    }
    std::unreachable();
}

auto EnumDomains::append(codegen::DeclarationHandle const declaration) -> EnumDomainId {
    auto const& store{*declarations_};
    auto const source{store.enums.index(declaration)};
    if (declarations_to_domains_[source].value != 0) {
        return declarations_to_domains_[source];
    }
    auto const row{static_cast<std::uint32_t>(values_.size())};
    auto const id{EnumDomainId{row + 1}};
    declarations_to_domains_[source] = id;
    values_.push_back(store.enums.values[source]);
    code_offsets_.push_back(static_cast<std::uint32_t>(codes_.size()));
    live_value_count.push_back(0);
    reserved_value_count.push_back(0);
    distinct_code_count.push_back(0);
    minimum_value.emplace_back();
    maximum_value.emplace_back();
    signedness.push_back(store.enums.signedness[source]);
    minimum_required_bits.emplace_back();
    count_value.push_back({});
    auto const issue_begin{static_cast<std::uint32_t>(issue_kinds_.size())};
    auto add_issue = [&](EnumDomainIssueKind kind, codegen::EnumeratorId value = {}) {
        issue_kinds_.push_back(kind);
        issue_values_.push_back(value);
    };
    std::optional<EnumCode> previous;
    bool has_prior_value{};
    bool all_codes_known{true};
    std::vector<EnumCode> distinct_codes;
    distinct_codes.reserve(values(id).size());
    auto const& enumerators{store.enumerators};
    auto const& strings{store.strings()};

    for (auto const value : values(id)) {
        auto const value_row{enumerators.index(value)};
        std::optional<EnumCode> code;
        if (auto const initializer{enumerators.initializer[value_row]}) {
            code = parse_enum_code(strings.view(*initializer));
            if (!code) {
                add_issue(EnumDomainIssueKind::non_literal, value);
            }
        } else if (!has_prior_value) {
            code = EnumCode{.negative = false, .magnitude = 0};
        } else if (previous) {
            code = next_enum_code(*previous);
            if (!code) {
                add_issue(EnumDomainIssueKind::implicit_overflow, value);
            }
        } else {
            add_issue(EnumDomainIssueKind::implicit_unknown, value);
        }
        has_prior_value = true;

        auto const is_count{store.enums.count[source] &&
                            enumerators.name[value_row] == *store.enums.count[source]};
        if (is_count) {
            count_value[row] = value;
        }
        if (enumerators.sentinel[value_row] || is_count) {
            ++reserved_value_count[row];
        } else {
            ++live_value_count[row];
        }
        if (code) {
            previous = code;
            distinct_codes.push_back(*code);
            if (!minimum_value[row] || enum_code_less(*code, *minimum_value[row])) {
                minimum_value[row] = code;
            }
            if (!maximum_value[row] || enum_code_less(*maximum_value[row], *code)) {
                maximum_value[row] = code;
            }
        } else {
            previous.reset();
            all_codes_known = false;
        }
        codes_.push_back(code);
    }

    std::ranges::sort(distinct_codes, enum_code_less);
    distinct_code_count[row] = static_cast<std::uint64_t>(
        std::unique(distinct_codes.begin(), distinct_codes.end()) - distinct_codes.begin());
    if (all_codes_known && minimum_value[row] && maximum_value[row]) {
        if (signedness[row] == EnumSignedness::unspecified) {
            signedness[row] =
                minimum_value[row]->negative ? EnumSignedness::signed_ : EnumSignedness::unsigned_;
        }
        auto const is_signed{signedness[row] == EnumSignedness::signed_};
        if (!is_signed && minimum_value[row]->negative) {
            add_issue(EnumDomainIssueKind::negative_unsigned);
        } else {
            minimum_required_bits[row] =
                minimum_enum_bits(*minimum_value[row], *maximum_value[row], is_signed);
            if (!minimum_required_bits[row]) {
                add_issue(EnumDomainIssueKind::width_overflow);
            }
        }
    }
    issue_ranges_.push_back(
        {issue_begin, static_cast<std::uint32_t>(issue_kinds_.size()) - issue_begin});
    return id;
}

auto analyze_enum_domains(codegen::Manifest const& manifest) -> EnumDomains {
    EnumDomains domains{manifest.declarations};
    auto const& store{manifest.declarations};
    for (auto const module : manifest.modules) {
        if (module.kind() != codegen::ModuleKind::normal) {
            continue;
        }
        for (auto const declaration :
             store.items(store.normal_modules.declarations[store.normal_modules.index(module)])) {
            if (declaration.kind() == codegen::DeclarationKind::enumeration) {
                domains.append(declaration);
            }
        }
    }
    return domains;
}

}
