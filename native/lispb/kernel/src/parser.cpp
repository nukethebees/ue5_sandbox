#include "parser.h"

#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/source_span.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <codegen/sexpr/token_kind.h>
#include <kernel_codegen/aliasing.h>
#include <kernel_codegen/binary_operator.h>
#include <kernel_codegen/constant_kind.h>
#include <kernel_codegen/document.h>
#include <kernel_codegen/expression_id.h>
#include <kernel_codegen/expression_kind.h>
#include <kernel_codegen/indices.h>
#include <kernel_codegen/operation_kind.h>
#include <kernel_codegen/scalar_type.h>
#include <kernel_codegen/storage_kind.h>
#include <kernel_codegen/variant_kind.h>
#include <lispb/indices.h>
#include <lispb/kernel_profile.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace kernel_codegen::detail {
namespace {

using lispb::codegen::sexpr::FormId;
using lispb::codegen::sexpr::SourceError;
using lispb::codegen::sexpr::SourceSpan;
using lispb::codegen::sexpr::TokenKind;

struct DecimalLiteral {
    std::string spelling;
    long double value;
};

struct OutputPath {
    lispb::KernelProfile profile;
    std::filesystem::path path;
    auto operator<=>(OutputPath const&) const = default;
};

struct VariantShape {
    VariantKind kind;
    std::optional<OperandId> target;
    auto operator<=>(VariantShape const&) const = default;
};

auto parse_decimal_literal(std::string_view const source) -> std::optional<DecimalLiteral> {
    if (source.empty()) {
        return std::nullopt;
    }

    std::size_t index{};
    if (source[index] == '+' || source[index] == '-') {
        ++index;
        if (index == source.size()) {
            return std::nullopt;
        }
    }

    auto const integer_start{index};
    while (index < source.size() && std::isdigit(static_cast<unsigned char>(source[index])) != 0) {
        ++index;
    }
    auto const integer_digits{index - integer_start};

    bool has_decimal_point{};
    if (index < source.size() && source[index] == '.') {
        has_decimal_point = true;
        ++index;
        auto const fraction_start{index};
        while (index < source.size() &&
               std::isdigit(static_cast<unsigned char>(source[index])) != 0) {
            ++index;
        }
        if (integer_digits == 0 && index == fraction_start) {
            return std::nullopt;
        }
    } else if (integer_digits == 0) {
        return std::nullopt;
    }

    bool has_exponent{};
    if (index < source.size() && (source[index] == 'e' || source[index] == 'E')) {
        has_exponent = true;
        ++index;
        if (index < source.size() && (source[index] == '+' || source[index] == '-')) {
            ++index;
        }
        auto const exponent_start{index};
        while (index < source.size() &&
               std::isdigit(static_cast<unsigned char>(source[index])) != 0) {
            ++index;
        }
        if (index == exponent_start) {
            return std::nullopt;
        }
    }
    if (index != source.size()) {
        return std::nullopt;
    }
    if (!has_decimal_point && !has_exponent && integer_digits > 1 && source[integer_start] == '0') {
        return std::nullopt;
    }

    std::string spelling{source};
    if (spelling.front() == '+') {
        spelling.erase(spelling.begin());
    }
    auto const sign_offset{spelling.front() == '-' ? std::size_t{1} : std::size_t{0}};
    if (spelling[sign_offset] == '.') {
        spelling.insert(sign_offset, 1, '0');
    }
    auto const decimal_point{spelling.find('.')};
    if (decimal_point != std::string::npos &&
        (decimal_point + 1 == spelling.size() || spelling[decimal_point + 1] == 'e' ||
         spelling[decimal_point + 1] == 'E')) {
        spelling.insert(decimal_point + 1, 1, '0');
    }

    long double value{};
    auto const [end, error]{std::from_chars(
        spelling.data(), spelling.data() + spelling.size(), value, std::chars_format::general)};
    if (error != std::errc{} || end != spelling.data() + spelling.size() || !std::isfinite(value)) {
        return std::nullopt;
    }
    return DecimalLiteral{std::move(spelling), value};
}

class Parser {
  public:
    Parser(lispb::codegen::sexpr::Syntax& syntax,
           Document& document,
           lispb::codegen::sexpr::SourceFileId const file,
           lispb::codegen::sexpr::HandleRange const forms)
        : syntax_{syntax}
        , document_{document}
        , file_{file}
        , forms_{forms} {}

    void parse() {
        auto const initial_modules{document_.modules.size()};
        std::set<lispb::IdentifierId> names;
        std::set<OutputPath> output_paths;
        for (auto const form : syntax_.forms(forms_)) {
            auto module{parse_module(form)};
            if (!names.insert(document_.modules.name(module)).second) {
                fail(document_.modules.span(module),
                     "duplicate kernel module '" + text(document_.modules.name(module)) + "'");
            }
            for (auto const& emission : document_.modules.emissions(module)) {
                std::vector<lispb::StringId> paths{document_.emissions.header(emission),
                                                   document_.emissions.source(emission)};
                if (document_.emissions.tests(emission)) {
                    paths.push_back(*document_.emissions.tests(emission));
                }
                for (auto const& output_path : paths) {
                    if (!output_paths
                             .insert({document_.emissions.profile(emission),
                                      document_.text(output_path)})
                             .second) {
                        fail(document_.emissions.span(emission),
                             "duplicate generated output path '" + document_.text(output_path) +
                                 "'");
                    }
                }
            }
        }
        if (document_.modules.size() == initial_modules) {
            fail(syntax_.sources().span(file_, 0, 0),
                 "document must contain at least one kernel-module");
        }
    }
  private:
    [[noreturn]] void fail(SourceSpan const& span, std::string const& message) const {
        throw SourceError{syntax_.sources(), span, message};
    }

    auto text(lispb::IdentifierId const id) const -> std::string {
        return std::string{syntax_.strings().view(id)};
    }

    auto head(FormId const form, std::string_view const context) const -> std::string {
        if (!syntax_.is_list(form) || syntax_.children(form).empty() ||
            syntax_.kind(syntax_.children(form).front()) != TokenKind::atom) {
            fail(syntax_.token_span(form),
                 std::string{"expected "} + std::string{context} + " form");
        }
        return std::string{syntax_.text(syntax_.children(form).front())};
    }

    auto atom(FormId const form, std::string_view const message) const -> std::string {
        if (syntax_.is_list(form) || syntax_.kind(form) != TokenKind::atom) {
            fail(syntax_.token_span(form), std::string{message});
        }
        return std::string{syntax_.text(form)};
    }

    auto string_value(FormId const form, std::string_view const message) const -> lispb::StringId {
        if (syntax_.is_list(form) || syntax_.kind(form) != TokenKind::string) {
            fail(syntax_.token_span(form), std::string{message});
        }
        return syntax_.text_id(form);
    }

    void require_size(FormId const form,
                      std::size_t const size,
                      std::string_view const usage) const {
        if (syntax_.children(form).size() != size) {
            fail(syntax_.token_span(form), "expected " + std::string{usage});
        }
    }

    static auto is_identifier(std::string_view const value) -> bool {
        static constexpr std::string_view cpp_keywords[]{
            "alignas",       "alignof",     "and",
            "and_eq",        "asm",         "auto",
            "bitand",        "bitor",       "bool",
            "break",         "case",        "catch",
            "char",          "char8_t",     "char16_t",
            "char32_t",      "class",       "compl",
            "concept",       "const",       "const_cast",
            "consteval",     "constexpr",   "constinit",
            "continue",      "co_await",    "co_return",
            "co_yield",      "decltype",    "default",
            "delete",        "do",          "double",
            "dynamic_cast",  "else",        "enum",
            "explicit",      "export",      "extern",
            "false",         "float",       "for",
            "friend",        "goto",        "if",
            "inline",        "int",         "long",
            "mutable",       "namespace",   "new",
            "noexcept",      "not",         "not_eq",
            "nullptr",       "operator",    "or",
            "or_eq",         "private",     "protected",
            "public",        "register",    "reinterpret_cast",
            "requires",      "return",      "short",
            "signed",        "sizeof",      "static",
            "static_assert", "static_cast", "struct",
            "switch",        "template",    "this",
            "thread_local",  "throw",       "true",
            "try",           "typedef",     "typeid",
            "typename",      "union",       "unsigned",
            "using",         "virtual",     "void",
            "volatile",      "wchar_t",     "while",
            "xor",           "xor_eq",
        };
        if (value.empty() || (std::isalpha(static_cast<unsigned char>(value.front())) == 0 &&
                              value.front() != '_')) {
            return false;
        }
        return std::ranges::find(cpp_keywords, value) == std::end(cpp_keywords) &&
               std::ranges::all_of(value, [](unsigned char const character) {
                   return std::isalnum(character) != 0 || character == '_';
               });
    }

    static auto is_qualified_identifier(std::string_view const value) -> bool {
        std::size_t start{};
        while (true) {
            auto const separator{value.find("::", start)};
            auto const part{value.substr(start, separator - start)};
            if (!is_identifier(part)) {
                return false;
            }
            if (separator == std::string::npos) {
                return true;
            }
            start = separator + 2;
        }
    }

    auto parse_expression(FormId const form,
                          OperationId const owner,
                          std::vector<bool>& used_operands) const -> ExpressionId {
        auto& expressions{document_.expressions};
        if (!syntax_.is_list(form)) {
            auto const value{atom(form, "expected operand reference or numeric literal")};
            if (is_identifier(value)) {
                auto const found{std::ranges::find_if(
                    document_.operations.operands(owner), [&](auto const& operand) {
                        return document_.operands.name(operand) == syntax_.identifier(form);
                    })};
                if (found == document_.operations.operands(owner).end()) {
                    fail(syntax_.token_span(form),
                         "expression references unknown operand '" + value + "'");
                }
                auto const index{static_cast<std::uint32_t>(
                    found - document_.operations.operands(owner).begin())};
                used_operands[index] = true;
                return expressions.reference(*found, syntax_.token_span(form));
            }
            auto const literal{parse_decimal_literal(value)};
            if (!literal) {
                fail(syntax_.token_span(form),
                     "expected operand reference or finite decimal literal, got '" + value + "'");
            }
            auto const spelling{literal->spelling == syntax_.text(form)
                                    ? syntax_.text_id(form)
                                    : syntax_.strings().add(literal->spelling)};
            return expressions.literal(spelling, literal->value, syntax_.token_span(form));
        }

        auto const operation{head(form, "expression")};
        if (operation == "constant") {
            require_size(form, 2, "'(constant nan|infinity|negative-infinity)'");
            auto const name{atom(syntax_.children(form)[1], "expected constant name")};
            ConstantKind constant;
            if (name == "nan") {
                constant = ConstantKind::nan;
            } else if (name == "infinity") {
                constant = ConstantKind::infinity;
            } else if (name == "negative-infinity") {
                constant = ConstantKind::negative_infinity;
            } else {
                fail(syntax_.token_span(syntax_.children(form)[1]),
                     "unknown constant '" + name + "'");
            }
            return expressions.constant(constant, syntax_.token_span(form));
        }

        require_size(form, 3, "binary expression '(operator lhs rhs)'");
        BinaryOperator binary;
        if (operation == "+") {
            binary = BinaryOperator::add;
        } else if (operation == "-") {
            binary = BinaryOperator::subtract;
        } else if (operation == "*") {
            binary = BinaryOperator::multiply;
        } else if (operation == "/") {
            binary = BinaryOperator::divide;
        } else {
            fail(syntax_.token_span(syntax_.children(form)[0]),
                 "unsupported expression operator '" + operation + "'");
        }
        auto const left{parse_expression(syntax_.children(form)[1], owner, used_operands)};
        auto const right{parse_expression(syntax_.children(form)[2], owner, used_operands)};
        return expressions.binary(binary, left, right, syntax_.token_span(form));
    }

    void parse_operation_expression(OperationId const operation,
                                    lispb::codegen::sexpr::FormId const form) const {
        std::vector<bool> used_operands(document_.operations.operands(operation).size());
        document_.operations.expression(operation) =
            parse_expression(form, operation, used_operands);
        auto const count{document_.operations.operands(operation).size()};
        for (std::size_t index{}; index < count; ++index) {
            if (!used_operands[index]) {
                auto const& operand{document_.operations.operands(operation)[index]};
                fail(document_.operands.span(operand),
                     "unused operand '" + text(document_.operands.name(operand)) + "'");
            }
        }
    }

    void parse_storage(FormId const form, OperandId const operand) const {
        auto parse_one = [&](FormId const item) {
            auto const& value{atom(item, "expected 'array' or 'scalar'")};
            if (value == "array") {
                return StorageKind::array;
            }
            if (value == "scalar") {
                return StorageKind::scalar;
            }
            fail(syntax_.token_span(item), "unknown operand storage '" + value + "'");
        };

        if (syntax_.is_list(form)) {
            if (syntax_.children(form).empty()) {
                fail(syntax_.token_span(form), "operand storage list must not be empty");
            }
            for (auto const& item : syntax_.children(form)) {
                auto const storage{parse_one(item)};
                if (std::ranges::find(document_.operands.storage(operand), storage) !=
                    document_.operands.storage(operand).end()) {
                    fail(syntax_.token_span(item), "duplicate operand storage");
                }
                document_.operands.append_storage(operand, storage);
            }
        } else {
            document_.operands.append_storage(operand, parse_one(form));
        }
    }

    auto parse_variant(FormId const form, OperationId const operation) const -> VariantId {
        auto const& form_head{head(form, "variant")};
        if (form_head == "out-of-place") {
            require_size(form, 2, "'(out-of-place public_name)'");
            auto const& public_name{
                atom(syntax_.children(form)[1], "expected public function name")};
            if (!is_identifier(public_name)) {
                fail(syntax_.token_span(syntax_.children(form)[1]),
                     "public function name must be a C++ identifier");
            }
            return document_.variants.add(VariantKind::out_of_place,
                                          syntax_.identifier(syntax_.children(form)[1]),
                                          std::nullopt,
                                          syntax_.token_span(form));
        }
        if (form_head == "in-place") {
            require_size(form, 3, "'(in-place target public_name)'");
            auto const& target{atom(syntax_.children(form)[1], "expected in-place target")};
            auto const& public_name{
                atom(syntax_.children(form)[2], "expected public function name")};
            if (!is_identifier(public_name)) {
                fail(syntax_.token_span(syntax_.children(form)[2]),
                     "public function name must be a C++ identifier");
            }
            auto const operand{std::ranges::find_if(
                document_.operations.operands(operation), [&](auto const& value) {
                    return document_.operands.name(value) ==
                           syntax_.identifier(syntax_.children(form)[1]);
                })};
            if (operand == document_.operations.operands(operation).end()) {
                fail(syntax_.token_span(form), "unknown in-place target '" + target + "'");
            }
            return document_.variants.add(VariantKind::in_place,
                                          syntax_.identifier(syntax_.children(form)[2]),
                                          *operand,
                                          syntax_.token_span(form));
        }
        fail(syntax_.token_span(form), "unknown variant '" + form_head + "'");
    }

    auto resolve_type_set(ModuleId const module, FormId const form) const -> TypeSetId {
        auto const name{atom(form, "expected type-set name")};
        auto const found{
            std::ranges::find_if(document_.modules.type_sets(module), [&](auto const& type_set) {
                return document_.type_sets.name(type_set) == syntax_.identifier(form);
            })};
        if (found == document_.modules.type_sets(module).end()) {
            fail(syntax_.token_span(form), "unknown type-set '" + name + "'");
        }
        return *found;
    }

    auto parse_operation(FormId const form, ModuleId const module) const -> OperationId {
        auto const form_head{head(form, "map or component-map")};
        if ((form_head != "map" && form_head != "component-map") ||
            syntax_.children(form).size() < 2) {
            fail(syntax_.token_span(form), "expected '(map|component-map name ...)'");
        }
        auto const is_component_map{form_head == "component-map"};
        auto const& name{atom(syntax_.children(form)[1], "expected map name")};
        if (!is_identifier(name)) {
            fail(syntax_.token_span(syntax_.children(form)[1]),
                 "map name must be a C++ identifier");
        }

        auto const result{document_.operations.add(is_component_map ? OperationKind::component_map
                                                                    : OperationKind::map,
                                                   syntax_.identifier(syntax_.children(form)[1]),
                                                   syntax_.token_span(form))};
        std::set<lispb::IdentifierId> fields;
        std::set<lispb::IdentifierId> operand_names;
        std::optional<lispb::codegen::sexpr::FormId> expression_form;
        std::optional<lispb::codegen::sexpr::FormId> variants_form;
        for (std::size_t index{2}; index < syntax_.children(form).size(); ++index) {
            auto const& field{syntax_.children(form)[index]};
            auto const& field_name{head(field, "map field")};
            if (field_name == "operand" || field_name == "component-operand") {
                if (field_name == "component-operand" && !is_component_map) {
                    fail(syntax_.token_span(field),
                         "component-operand is valid only in component-map");
                }
                require_size(field, 3, "'(operand name storage)'");
                auto operand_name{atom(syntax_.children(field)[1], "expected operand name")};
                if (!is_identifier(operand_name)) {
                    fail(syntax_.token_span(syntax_.children(field)[1]),
                         "operand name must be a C++ identifier");
                }
                if (!operand_names.insert(syntax_.identifier(syntax_.children(field)[1])).second) {
                    fail(syntax_.token_span(syntax_.children(field)[1]),
                         "duplicate operand '" + operand_name + "'");
                }
                auto const operand{
                    document_.operands.add(syntax_.identifier(syntax_.children(field)[1]),
                                           syntax_.token_span(field),
                                           field_name == "component-operand")};
                parse_storage(syntax_.children(field)[2], operand);
                document_.operations.append_operands(result, operand);
                continue;
            }
            if (!fields.insert(syntax_.identifier(syntax_.children(field).front())).second) {
                fail(syntax_.token_span(field), "duplicate map field '" + field_name + "'");
            }
            if (field_name == "components") {
                if (!is_component_map || syntax_.children(field).size() < 3) {
                    fail(syntax_.token_span(field),
                         "component-map requires at least two component names");
                }
                std::set<lispb::IdentifierId> components;
                for (std::size_t component_index{1};
                     component_index < syntax_.children(field).size();
                     ++component_index) {
                    auto component{
                        atom(syntax_.children(field)[component_index], "expected component name")};
                    if (!is_identifier(component) ||
                        !components
                             .insert(syntax_.identifier(syntax_.children(field)[component_index]))
                             .second) {
                        fail(syntax_.token_span(syntax_.children(field)[component_index]),
                             "component names must be unique C++ identifiers");
                    }
                    document_.operations.append_components(
                        result, syntax_.identifier(syntax_.children(field)[component_index]));
                }
            } else if (field_name == "types") {
                require_size(field, 2, "'(types type_set)'");
                document_.operations.type_set(result) =
                    resolve_type_set(module, syntax_.children(field)[1]);
            } else if (field_name == "output") {
                require_size(field, 2, "'(output name)'");
                static_cast<void>(atom(syntax_.children(field)[1], "expected output name"));
                document_.operations.output(result) =
                    syntax_.identifier(syntax_.children(field)[1]);
                if (!is_identifier(text(document_.operations.output(result)))) {
                    fail(syntax_.token_span(syntax_.children(field)[1]),
                         "output name must be a C++ identifier");
                }
            } else if (field_name == "expression") {
                require_size(field, 2, "'(expression expression)'");
                expression_form = syntax_.children(field)[1];
            } else if (field_name == "variants") {
                if (syntax_.children(field).size() < 2) {
                    fail(syntax_.token_span(field), "variants must not be empty");
                }
                variants_form = field;
            } else if (field_name == "aliasing") {
                require_size(field, 2, "'(aliasing output-disjoint|pairwise-disjoint)'");
                auto const& policy{atom(syntax_.children(field)[1], "expected aliasing policy")};
                if (policy == "output-disjoint") {
                    document_.operations.aliasing(result) = Aliasing::output_disjoint;
                } else if (policy == "pairwise-disjoint") {
                    document_.operations.aliasing(result) = Aliasing::pairwise_disjoint;
                } else {
                    fail(syntax_.token_span(syntax_.children(field)[1]),
                         "unknown aliasing policy '" + policy + "'");
                }
            } else {
                fail(syntax_.token_span(field), "unknown map field '" + field_name + "'");
            }
        }

        if (document_.operations.type_set(result).value == 0xffffffff ||
            document_.operations.output(result).value == 0 ||
            document_.operations.operands(result).empty() || !variants_form || !expression_form) {
            fail(syntax_.token_span(form),
                 "map requires types, operands, output, expression, and variants");
        }
        if (is_component_map &&
            (document_.operations.components(result).empty() ||
             std::ranges::none_of(document_.operations.operands(result), [&](auto const operand) {
                 return document_.operands.is_component(operand);
             }))) {
            fail(syntax_.token_span(form),
                 "component-map requires components and at least one component-operand");
        }
        if (operand_names.contains(document_.operations.output(result))) {
            fail(syntax_.token_span(form), "output name must differ from operand names");
        }

        parse_operation_expression(result, *expression_form);
        auto const variants{*variants_form};
        auto const variant_count{syntax_.children(variants).size()};
        for (std::size_t index{1}; index < variant_count; ++index) {
            document_.operations.append_variants(
                result, parse_variant(syntax_.children(variants)[index], result));
        }

        std::set<VariantShape> variant_shapes;
        for (auto const& variant : document_.operations.variants(result)) {
            if (!variant_shapes
                     .insert({document_.variants.kind(variant), document_.variants.target(variant)})
                     .second) {
                auto const shape{document_.variants.kind(variant) == VariantKind::out_of_place
                                     ? std::string{"out-of-place"}
                                     : "in-place:" + text(document_.operands.name(
                                                         *document_.variants.target(variant)))};
                fail(document_.variants.span(variant),
                     "duplicate generated variant '" + shape + "'");
            }
            if (document_.variants.kind(variant) == VariantKind::out_of_place) {
                auto const has_array_operand{std::ranges::any_of(
                    document_.operations.operands(result), [&](auto const operand) {
                        return std::ranges::find(document_.operands.storage(operand),
                                                 StorageKind::array) !=
                               document_.operands.storage(operand).end();
                    })};
                if (!has_array_operand) {
                    fail(document_.variants.span(variant),
                         "out-of-place map must allow at least one array operand");
                }
            } else if (is_component_map) {
                if (!document_.operands.is_component(*document_.variants.target(variant))) {
                    fail(document_.variants.span(variant),
                         "component-map in-place target must be a component-operand");
                }
            }
        }
        return result;
    }

    void validate_expression_type(ExpressionId const expression, ScalarType const type) const {
        if (expression.kind() == ExpressionKind::constant &&
            (type == ScalarType::int32 || type == ScalarType::uint32)) {
            fail(document_.expressions.span(expression),
                 "constant is not supported by integral concrete type '" + type_spelling(type) +
                     "'");
        }
        if (expression.kind() == ExpressionKind::literal) {
            auto const value{document_.expressions.number(expression)};
            if (type == ScalarType::int32) {
                if (std::trunc(value) != value ||
                    value < static_cast<long double>(std::numeric_limits<std::int32_t>::min()) ||
                    value > static_cast<long double>(std::numeric_limits<std::int32_t>::max())) {
                    fail(document_.expressions.span(expression),
                         "literal '" + std::string{document_.expressions.literal(expression)} +
                             "' is not representable as int32");
                }
            } else if (type == ScalarType::uint32) {
                if (std::trunc(value) != value || value < 0.0L ||
                    value > static_cast<long double>(std::numeric_limits<std::uint32_t>::max())) {
                    fail(document_.expressions.span(expression),
                         "literal '" + std::string{document_.expressions.literal(expression)} +
                             "' is not representable as uint32");
                }
            } else if (type == ScalarType::float32) {
                auto const magnitude{std::abs(value)};
                if (magnitude > static_cast<long double>(std::numeric_limits<float>::max()) ||
                    (magnitude != 0.0L &&
                     magnitude <
                         static_cast<long double>(std::numeric_limits<float>::denorm_min()))) {
                    fail(document_.expressions.span(expression),
                         "literal '" + std::string{document_.expressions.literal(expression)} +
                             "' is not representable as float");
                }
            } else if (type == ScalarType::float64) {
                auto const magnitude{std::abs(value)};
                if (magnitude > static_cast<long double>(std::numeric_limits<double>::max()) ||
                    (magnitude != 0.0L &&
                     magnitude <
                         static_cast<long double>(std::numeric_limits<double>::denorm_min()))) {
                    fail(document_.expressions.span(expression),
                         "literal '" + std::string{document_.expressions.literal(expression)} +
                             "' is not representable as double");
                }
            }
        }
        if (expression.kind() == ExpressionKind::binary) {
            validate_expression_type(document_.expressions.left(expression), type);
            validate_expression_type(document_.expressions.right(expression), type);
        }
    }

    auto parse_scalar_type(FormId const form) const -> ScalarType {
        auto const type{atom(form, "expected concrete type")};
        if (type == "int32") {
            return ScalarType::int32;
        }
        if (type == "uint32") {
            return ScalarType::uint32;
        }
        if (type == "float") {
            return ScalarType::float32;
        }
        if (type == "double") {
            return ScalarType::float64;
        }
        fail(syntax_.token_span(form), "unsupported concrete type '" + type + "'");
    }

    auto parse_type_set(FormId const form) const -> TypeSetId {
        if (head(form, "type-set") != "type-set" || syntax_.children(form).size() < 3) {
            fail(syntax_.token_span(form), "expected '(type-set name type ...)'");
        }
        auto const& name{atom(syntax_.children(form)[1], "expected type-set name")};
        if (!is_identifier(name)) {
            fail(syntax_.token_span(syntax_.children(form)[1]),
                 "type-set name must be an identifier");
        }
        auto const result{document_.type_sets.add(syntax_.identifier(syntax_.children(form)[1]),
                                                  syntax_.token_span(form))};
        std::set<ScalarType> types;
        for (std::size_t index{2}; index < syntax_.children(form).size(); ++index) {
            auto const type{parse_scalar_type(syntax_.children(form)[index])};
            if (!types.insert(type).second) {
                fail(syntax_.token_span(syntax_.children(form)[index]),
                     "duplicate concrete type '" + type_spelling(type) + "'");
            }
            document_.type_sets.append_types(result, type);
        }
        return result;
    }

    void validate_output_path(std::filesystem::path const& path, SourceSpan const& span) const {
        auto const escapes{std::ranges::any_of(path.lexically_normal(),
                                               [](auto const& part) { return part == ".."; })};
        if (path.empty() || path.is_absolute() || path.has_root_path() || escapes) {
            fail(span, "generated output paths must remain inside the output root");
        }
    }

    auto parse_emission(FormId const form) const -> EmissionId {
        if (head(form, "emit") != "emit" || syntax_.children(form).size() < 2) {
            fail(syntax_.token_span(form), "expected '(emit unreal|standard ...)'");
        }
        auto const& profile_name{atom(syntax_.children(form)[1], "expected emission profile")};
        lispb::KernelProfile profile;
        if (profile_name == "unreal") {
            profile = lispb::KernelProfile::unreal;
        } else if (profile_name == "standard") {
            profile = lispb::KernelProfile::standard;
        } else {
            fail(syntax_.token_span(syntax_.children(form)[1]),
                 "unknown emission profile '" + profile_name + "'");
        }

        auto const result{document_.emissions.add(profile, syntax_.token_span(form))};
        std::set<lispb::IdentifierId> fields;
        for (std::size_t index{2}; index < syntax_.children(form).size(); ++index) {
            auto const& field{syntax_.children(form)[index]};
            auto const& field_name{head(field, "emission field")};
            if (!fields.insert(syntax_.identifier(syntax_.children(field).front())).second) {
                fail(syntax_.token_span(field), "duplicate emission field '" + field_name + "'");
            }
            require_size(field, 2, "single-value emission field");
            if (field_name == "header") {
                document_.emissions.header(result) =
                    string_value(syntax_.children(field)[1], "expected header path string");
            } else if (field_name == "source") {
                document_.emissions.source(result) =
                    string_value(syntax_.children(field)[1], "expected source path string");
            } else if (field_name == "tests") {
                document_.emissions.tests(result) =
                    string_value(syntax_.children(field)[1], "expected test path string");
            } else if (field_name == "header-include") {
                document_.emissions.header_include(result) =
                    string_value(syntax_.children(field)[1], "expected header include string");
            } else if (field_name == "namespace") {
                static_cast<void>(atom(syntax_.children(field)[1], "expected C++ namespace"));
                document_.emissions.cpp_namespace(result) =
                    syntax_.identifier(syntax_.children(field)[1]);
                if (!is_qualified_identifier(text(document_.emissions.cpp_namespace(result)))) {
                    fail(syntax_.token_span(syntax_.children(field)[1]),
                         "namespace must be a qualified C++ identifier");
                }
            } else if (field_name == "export") {
                static_cast<void>(atom(syntax_.children(field)[1], "expected export specifier"));
                document_.emissions.export_specifier(result) =
                    syntax_.identifier(syntax_.children(field)[1]);
                if (!is_identifier(text(document_.emissions.export_specifier(result)))) {
                    fail(syntax_.token_span(syntax_.children(field)[1]),
                         "export specifier must be an identifier");
                }
            } else {
                fail(syntax_.token_span(field), "unknown emission field '" + field_name + "'");
            }
        }
        if (document_.emissions.header(result).value == 0 ||
            document_.emissions.source(result).value == 0 ||
            document_.strings.view(document_.emissions.header_include(result)).empty() ||
            document_.emissions.cpp_namespace(result).value == 0) {
            fail(syntax_.token_span(form),
                 "emit requires header, source, header-include, and namespace");
        }
        if (profile == lispb::KernelProfile::unreal &&
            document_.emissions.export_specifier(result).value == 0) {
            fail(syntax_.token_span(form), "unreal emission requires export");
        }
        if (profile == lispb::KernelProfile::unreal && document_.emissions.tests(result)) {
            fail(syntax_.token_span(form),
                 "generated tests are supported only by the standard profile");
        }
        if (profile == lispb::KernelProfile::standard &&
            document_.emissions.export_specifier(result).value != 0) {
            fail(syntax_.token_span(form), "export is supported only by the unreal profile");
        }
        validate_output_path(document_.text(document_.emissions.header(result)),
                             document_.emissions.span(result));
        validate_output_path(document_.text(document_.emissions.source(result)),
                             document_.emissions.span(result));
        if (document_.emissions.tests(result)) {
            validate_output_path(document_.text(*document_.emissions.tests(result)),
                                 document_.emissions.span(result));
        }
        return result;
    }

    auto parse_module(FormId const form) const -> ModuleId {
        if (head(form, "kernel-module") != "kernel-module" || syntax_.children(form).size() < 2) {
            fail(syntax_.token_span(form), "expected '(kernel-module name ...)'");
        }
        auto const& name{atom(syntax_.children(form)[1], "expected kernel module name")};
        if (!is_identifier(name)) {
            fail(syntax_.token_span(syntax_.children(form)[1]),
                 "kernel module name must be an identifier");
        }
        auto const result{document_.modules.add(syntax_.identifier(syntax_.children(form)[1]),
                                                syntax_.token_span(form))};
        std::set<lispb::IdentifierId> fields;
        std::set<lispb::KernelProfile> profiles;
        std::set<lispb::IdentifierId> type_sets;
        std::set<lispb::IdentifierId> operation_names;
        auto const field_count{syntax_.children(form).size()};
        for (std::size_t index{2}; index < field_count; ++index) {
            auto const field{syntax_.children(form)[index]};
            if (head(field, "kernel module field") == "type-set") {
                auto type_set{parse_type_set(field)};
                if (!type_sets.insert(document_.type_sets.name(type_set)).second) {
                    fail(document_.type_sets.span(type_set),
                         "duplicate type-set '" + text(document_.type_sets.name(type_set)) + "'");
                }
                document_.modules.append_type_sets(result, type_set);
            }
        }
        for (std::size_t index{2}; index < syntax_.children(form).size(); ++index) {
            auto const& field{syntax_.children(form)[index]};
            auto const& field_name{head(field, "kernel module field")};
            if (field_name == "type-set") {
                continue;
            }
            if (field_name == "map" || field_name == "component-map") {
                auto operation{parse_operation(field, result)};
                if (!operation_names.insert(document_.operations.name(operation)).second) {
                    fail(document_.operations.span(operation),
                         "duplicate operation '" + text(document_.operations.name(operation)) +
                             "'");
                }
                document_.modules.append_operations(result, operation);
                continue;
            }
            if (field_name == "emit") {
                auto emission{parse_emission(field)};
                if (!profiles.insert(document_.emissions.profile(emission)).second) {
                    fail(document_.emissions.span(emission), "duplicate emission profile");
                }
                document_.modules.append_emissions(result, emission);
                continue;
            }
            if (!fields.insert(syntax_.identifier(syntax_.children(field).front())).second) {
                fail(syntax_.token_span(field),
                     "duplicate kernel module field '" + field_name + "'");
            }
            fail(syntax_.token_span(field), "unknown kernel module field '" + field_name + "'");
        }
        if (document_.modules.emissions(result).empty() ||
            document_.modules.type_sets(result).empty() ||
            document_.modules.operations(result).empty()) {
            fail(syntax_.token_span(form),
                 "kernel-module requires emissions, type sets, and operations");
        }
        auto const has_component_map{
            std::ranges::any_of(document_.modules.operations(result), [](auto const id) {
                return id.kind() == OperationKind::component_map;
            })};
        if (has_component_map &&
            std::ranges::any_of(document_.modules.emissions(result), [&](auto const emission) {
                return document_.emissions.profile(emission) != lispb::KernelProfile::standard;
            })) {
            fail(syntax_.token_span(form),
                 "component-map operations are supported only by standard emissions");
        }
        for (auto const operation : document_.modules.operations(result)) {
            auto const operation_types{document_.operations.type_set(operation)};
            for (auto const& type : document_.type_sets.types(operation_types)) {
                validate_expression_type(document_.operations.expression(operation), type);
            }
            for (auto const& variant : document_.operations.variants(operation)) {
                if (document_.variants.kind(variant) != VariantKind::in_place) {
                    continue;
                }
                auto const target{*document_.variants.target(variant)};
                if (std::ranges::find(document_.operands.storage(target), StorageKind::array) ==
                    document_.operands.storage(target).end()) {
                    fail(document_.variants.span(variant),
                         "in-place target must allow array storage");
                }
            }
        }
        document_.modules.expanded_variants(result) =
            document_.expanded_variants.expand(document_, result);
        return result;
    }

    lispb::codegen::sexpr::Syntax& syntax_;
    Document& document_;
    lispb::codegen::sexpr::SourceFileId file_;
    lispb::codegen::sexpr::HandleRange forms_;
};

}

void parse(lispb::codegen::sexpr::Syntax& syntax,
           Document& document,
           lispb::codegen::sexpr::SourceFileId const file) {
    Parser{syntax, document, file, lispb::codegen::sexpr::read_forms(syntax, file)}.parse();
}

}
