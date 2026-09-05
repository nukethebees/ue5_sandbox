#include "parser.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace kernel_codegen::detail {
namespace {

using codegen::sexpr::SourceError;
using codegen::sexpr::SourceSpan;
using codegen::sexpr::Token;
using codegen::sexpr::TokenKind;

struct Form {
    Token token;
    std::vector<Form> children;

    auto is_list() const -> bool { return token.kind == TokenKind::left_parenthesis; }
};

struct DecimalLiteral {
    std::string spelling;
    long double value;
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
    if (!has_decimal_point && !has_exponent && integer_digits > 1 &&
        source[integer_start] == '0') {
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
    auto const [end, error]{std::from_chars(spelling.data(),
                                            spelling.data() + spelling.size(),
                                            value,
                                            std::chars_format::general)};
    if (error != std::errc{} || end != spelling.data() + spelling.size() ||
        !std::isfinite(value)) {
        return std::nullopt;
    }
    return DecimalLiteral{std::move(spelling), value};
}

class Parser {
  public:
    Parser(std::string_view const path, std::vector<Token> tokens)
        : path_{path}, tokens_{std::move(tokens)} {}

    auto parse() -> Document {
        auto const forms{read_document()};
        Document result;
        std::set<std::string> names;
        std::set<std::string> output_paths;
        for (auto const& form : forms) {
            auto module{parse_module(form)};
            if (!names.insert(module.name).second) {
                fail(module.span, "duplicate kernel module '" + module.name + "'");
            }
            for (auto const& emission : module.emissions) {
                std::vector<std::filesystem::path> paths{emission.header, emission.source};
                if (emission.tests) {
                    paths.push_back(*emission.tests);
                }
                for (auto const& output_path : paths) {
                    auto const key{std::to_string(static_cast<int>(emission.profile)) + ":" +
                                   output_path.generic_string()};
                    if (!output_paths.insert(key).second) {
                        fail(emission.span,
                             "duplicate generated output path '" +
                                 output_path.generic_string() + "'");
                    }
                }
            }
            result.modules.push_back(std::move(module));
        }
        if (result.modules.empty()) {
            fail(tokens_.back().span, "document must contain at least one kernel-module");
        }
        return result;
    }

  private:
    [[noreturn]] void fail(SourceSpan const span, std::string const& message) const {
        throw SourceError{path_, span, message};
    }

    auto read_form(std::size_t& index) const -> Form {
        auto const token{tokens_[index++]};
        if (token.kind == TokenKind::right_parenthesis || token.kind == TokenKind::end) {
            fail(token.span, "expected an expression");
        }
        Form result{token, {}};
        if (!result.is_list()) {
            return result;
        }
        while (tokens_[index].kind != TokenKind::right_parenthesis) {
            if (tokens_[index].kind == TokenKind::end) {
                fail(token.span, "expected ')' after expression");
            }
            result.children.push_back(read_form(index));
        }
        ++index;
        return result;
    }

    auto read_document() const -> std::vector<Form> {
        std::vector<Form> result;
        std::size_t index{};
        while (tokens_[index].kind != TokenKind::end) {
            result.push_back(read_form(index));
        }
        return result;
    }

    auto head(Form const& form, std::string_view const context) const -> std::string const& {
        if (!form.is_list() || form.children.empty() ||
            form.children.front().token.kind != TokenKind::atom) {
            fail(form.token.span, std::string{"expected "} + std::string{context} + " form");
        }
        return form.children.front().token.text;
    }

    auto atom(Form const& form, std::string_view const message) const -> std::string const& {
        if (form.is_list() || form.token.kind != TokenKind::atom) {
            fail(form.token.span, std::string{message});
        }
        return form.token.text;
    }

    auto string_value(Form const& form, std::string_view const message) const
        -> std::string const& {
        if (form.is_list() || form.token.kind != TokenKind::string) {
            fail(form.token.span, std::string{message});
        }
        return form.token.text;
    }

    void require_size(Form const& form, std::size_t const size, std::string_view const usage) const {
        if (form.children.size() != size) {
            fail(form.token.span, "expected " + std::string{usage});
        }
    }

    static auto is_identifier(std::string const& value) -> bool {
        static std::set<std::string> const cpp_keywords{
            "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool",
            "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t", "class",
            "compl", "concept", "const", "const_cast", "consteval", "constexpr", "constinit",
            "continue", "co_await", "co_return", "co_yield", "decltype", "default", "delete",
            "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern",
            "false", "float", "for", "friend", "goto", "if", "inline", "int", "long",
            "mutable", "namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator",
            "or", "or_eq", "private", "protected", "public", "register", "reinterpret_cast",
            "requires", "return", "short", "signed", "sizeof", "static", "static_assert",
            "static_cast", "struct", "switch", "template", "this", "thread_local", "throw",
            "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using",
            "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq",
        };
        if (value.empty() || (std::isalpha(static_cast<unsigned char>(value.front())) == 0 &&
                              value.front() != '_')) {
            return false;
        }
        return !cpp_keywords.contains(value) &&
               std::ranges::all_of(value, [](unsigned char const character) {
                   return std::isalnum(character) != 0 || character == '_';
               });
    }

    static auto is_qualified_identifier(std::string const& value) -> bool {
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

    auto parse_expression(Form const& form) const -> Expression {
        if (!form.is_list()) {
            auto const& value{atom(form, "expected operand reference or numeric literal")};
            if (is_identifier(value)) {
                return Expression{ExpressionKind::reference, value, {}, form.token.span};
            }
            auto literal{parse_decimal_literal(value)};
            if (!literal) {
                fail(form.token.span,
                     "expected operand reference or finite decimal literal, got '" + value + "'");
            }
            return Expression{ExpressionKind::literal,
                              std::move(literal->spelling),
                              {},
                              form.token.span};
        }

        auto const& operation{head(form, "expression")};
        if (operation == "constant") {
            require_size(form, 2, "'(constant nan|infinity|negative-infinity)'");
            auto const& name{atom(form.children[1], "expected constant name")};
            ConstantKind constant;
            if (name == "nan") {
                constant = ConstantKind::nan;
            } else if (name == "infinity") {
                constant = ConstantKind::infinity;
            } else if (name == "negative-infinity") {
                constant = ConstantKind::negative_infinity;
            } else {
                fail(form.children[1].token.span, "unknown constant '" + name + "'");
            }
            return Expression{ExpressionKind::constant, {}, {}, form.token.span, constant};
        }

        require_size(form, 3, "binary expression '(operator lhs rhs)'");
        if (operation != "+" && operation != "-" && operation != "*" && operation != "/") {
            fail(form.children[0].token.span, "unsupported expression operator '" + operation + "'");
        }
        std::vector<Expression> arguments;
        arguments.push_back(parse_expression(form.children[1]));
        arguments.push_back(parse_expression(form.children[2]));
        return Expression{ExpressionKind::binary, operation, std::move(arguments), form.token.span};
    }

    auto parse_storage(Form const& form) const -> std::vector<StorageKind> {
        auto parse_one = [&](Form const& item) {
            auto const& value{atom(item, "expected 'array' or 'scalar'")};
            if (value == "array") {
                return StorageKind::array;
            }
            if (value == "scalar") {
                return StorageKind::scalar;
            }
            fail(item.token.span, "unknown operand storage '" + value + "'");
        };

        std::vector<StorageKind> result;
        if (form.is_list()) {
            if (form.children.empty()) {
                fail(form.token.span, "operand storage list must not be empty");
            }
            for (auto const& item : form.children) {
                auto const storage{parse_one(item)};
                if (std::ranges::find(result, storage) != result.end()) {
                    fail(item.token.span, "duplicate operand storage");
                }
                result.push_back(storage);
            }
        } else {
            result.push_back(parse_one(form));
        }
        return result;
    }

    auto parse_variant(Form const& form) const -> Variant {
        auto const& form_head{head(form, "variant")};
        if (form_head == "out-of-place") {
            require_size(form, 2, "'(out-of-place public_name)'");
            auto const& public_name{atom(form.children[1], "expected public function name")};
            if (!is_identifier(public_name)) {
                fail(form.children[1].token.span, "public function name must be a C++ identifier");
            }
            return Variant{VariantKind::out_of_place, public_name, std::nullopt, form.token.span};
        }
        if (form_head == "in-place") {
            require_size(form, 3, "'(in-place target public_name)'");
            auto const& target{atom(form.children[1], "expected in-place target")};
            auto const& public_name{atom(form.children[2], "expected public function name")};
            if (!is_identifier(public_name)) {
                fail(form.children[2].token.span, "public function name must be a C++ identifier");
            }
            return Variant{VariantKind::in_place, public_name, target, form.token.span};
        }
        fail(form.token.span, "unknown variant '" + form_head + "'");
    }

    auto parse_operation(Form const& form) const -> MapOperation {
        if (head(form, "map") != "map" || form.children.size() < 2) {
            fail(form.token.span, "expected '(map name ...)'");
        }
        auto const& name{atom(form.children[1], "expected map name")};
        if (!is_identifier(name)) {
            fail(form.children[1].token.span, "map name must be a C++ identifier");
        }

        MapOperation result{.name = name,
                            .expression = Expression{ExpressionKind::literal, {}, {}, {}},
                            .span = form.token.span};
        std::set<std::string> fields;
        std::set<std::string> operand_names;
        bool has_expression{false};
        for (std::size_t index{2}; index < form.children.size(); ++index) {
            auto const& field{form.children[index]};
            auto const& field_name{head(field, "map field")};
            if (field_name == "operand") {
                require_size(field, 3, "'(operand name storage)'");
                auto operand_name{atom(field.children[1], "expected operand name")};
                if (!is_identifier(operand_name)) {
                    fail(field.children[1].token.span, "operand name must be a C++ identifier");
                }
                if (!operand_names.insert(operand_name).second) {
                    fail(field.children[1].token.span, "duplicate operand '" + operand_name + "'");
                }
                result.operands.push_back(Operand{std::move(operand_name),
                                                  parse_storage(field.children[2]),
                                                  field.token.span});
                continue;
            }
            if (!fields.insert(field_name).second) {
                fail(field.token.span, "duplicate map field '" + field_name + "'");
            }
            if (field_name == "types") {
                require_size(field, 2, "'(types type_set)'");
                result.type_set = atom(field.children[1], "expected type-set name");
            } else if (field_name == "output") {
                require_size(field, 2, "'(output name)'");
                result.output = atom(field.children[1], "expected output name");
                if (!is_identifier(result.output)) {
                    fail(field.children[1].token.span, "output name must be a C++ identifier");
                }
            } else if (field_name == "expression") {
                require_size(field, 2, "'(expression expression)'");
                result.expression = parse_expression(field.children[1]);
                has_expression = true;
            } else if (field_name == "variants") {
                if (field.children.size() < 2) {
                    fail(field.token.span, "variants must not be empty");
                }
                for (std::size_t variant_index{1}; variant_index < field.children.size();
                     ++variant_index) {
                    result.variants.push_back(parse_variant(field.children[variant_index]));
                }
            } else if (field_name == "aliasing") {
                require_size(field, 2, "'(aliasing output-disjoint|pairwise-disjoint)'");
                auto const& policy{atom(field.children[1], "expected aliasing policy")};
                if (policy == "output-disjoint") {
                    result.aliasing = Aliasing::output_disjoint;
                } else if (policy == "pairwise-disjoint") {
                    result.aliasing = Aliasing::pairwise_disjoint;
                } else {
                    fail(field.children[1].token.span, "unknown aliasing policy '" + policy + "'");
                }
            } else {
                fail(field.token.span, "unknown map field '" + field_name + "'");
            }
        }

        if (result.type_set.empty() || result.output.empty() || result.operands.empty() ||
            result.variants.empty() || !has_expression) {
            fail(form.token.span,
                 "map requires types, operands, output, expression, and variants");
        }
        if (operand_names.contains(result.output)) {
            fail(form.token.span, "output name must differ from operand names");
        }

        std::set<std::string> references;
        auto collect_references = [&](auto const& self, Expression const& expression) -> void {
            if (expression.kind == ExpressionKind::reference) {
                references.insert(expression.value);
            }
            for (auto const& argument : expression.arguments) {
                self(self, argument);
            }
        };
        collect_references(collect_references, result.expression);
        for (auto const& reference : references) {
            if (!operand_names.contains(reference)) {
                fail(result.expression.span, "expression references unknown operand '" + reference + "'");
            }
        }
        for (auto const& operand : result.operands) {
            if (!references.contains(operand.name)) {
                fail(operand.span, "unused operand '" + operand.name + "'");
            }
        }

        std::set<std::string> variant_shapes;
        for (auto const& variant : result.variants) {
            auto const shape{variant.kind == VariantKind::out_of_place
                                 ? std::string{"out-of-place"}
                                 : "in-place:" + *variant.target};
            if (!variant_shapes.insert(shape).second) {
                fail(variant.span, "duplicate generated variant '" + shape + "'");
            }
            if (variant.kind == VariantKind::out_of_place) {
                auto const has_array_operand{
                    std::ranges::any_of(result.operands, [](auto const& operand) {
                        return std::ranges::find(operand.storage, StorageKind::array) !=
                               operand.storage.end();
                    })};
                if (!has_array_operand) {
                    fail(variant.span, "out-of-place map must allow at least one array operand");
                }
            } else if (!operand_names.contains(*variant.target)) {
                fail(variant.span, "unknown in-place target '" + *variant.target + "'");
            }
        }
        return result;
    }

    void validate_expression_type(Expression const& expression, std::string const& type) const {
        if (expression.kind == ExpressionKind::constant &&
            (type == "int32" || type == "uint32")) {
            fail(expression.span,
                 "constant is not supported by integral concrete type '" + type + "'");
        }
        if (expression.kind == ExpressionKind::literal) {
            auto const literal{parse_decimal_literal(expression.value)};
            if (!literal) {
                fail(expression.span, "invalid finite decimal literal '" + expression.value + "'");
            }
            auto const value{literal->value};
            if (type == "int32") {
                if (std::trunc(value) != value ||
                    value < static_cast<long double>(std::numeric_limits<std::int32_t>::min()) ||
                    value > static_cast<long double>(std::numeric_limits<std::int32_t>::max())) {
                    fail(expression.span,
                         "literal '" + expression.value + "' is not representable as int32");
                }
            } else if (type == "uint32") {
                if (std::trunc(value) != value || value < 0.0L ||
                    value > static_cast<long double>(std::numeric_limits<std::uint32_t>::max())) {
                    fail(expression.span,
                         "literal '" + expression.value + "' is not representable as uint32");
                }
            } else if (type == "float") {
                auto const magnitude{std::abs(value)};
                if (magnitude > static_cast<long double>(std::numeric_limits<float>::max()) ||
                    (magnitude != 0.0L &&
                     magnitude < static_cast<long double>(std::numeric_limits<float>::denorm_min()))) {
                    fail(expression.span,
                         "literal '" + expression.value + "' is not representable as float");
                }
            } else if (type == "double") {
                auto const magnitude{std::abs(value)};
                if (magnitude > static_cast<long double>(std::numeric_limits<double>::max()) ||
                    (magnitude != 0.0L && magnitude < static_cast<long double>(
                                                           std::numeric_limits<double>::denorm_min()))) {
                    fail(expression.span,
                         "literal '" + expression.value + "' is not representable as double");
                }
            }
        }
        for (auto const& argument : expression.arguments) {
            validate_expression_type(argument, type);
        }
    }

    auto parse_type_set(Form const& form) const -> TypeSet {
        if (head(form, "type-set") != "type-set" || form.children.size() < 3) {
            fail(form.token.span, "expected '(type-set name type ...)'");
        }
        auto const& name{atom(form.children[1], "expected type-set name")};
        if (!is_identifier(name)) {
            fail(form.children[1].token.span, "type-set name must be an identifier");
        }
        TypeSet result{.name = name, .span = form.token.span};
        std::set<std::string> types;
        for (std::size_t index{2}; index < form.children.size(); ++index) {
            auto const& type{atom(form.children[index], "expected concrete type")};
            if (type != "int32" && type != "uint32" && type != "float" && type != "double") {
                fail(form.children[index].token.span, "unsupported concrete type '" + type + "'");
            }
            if (!types.insert(type).second) {
                fail(form.children[index].token.span, "duplicate concrete type '" + type + "'");
            }
            result.types.push_back(type);
        }
        return result;
    }

    void validate_output_path(std::filesystem::path const& path, SourceSpan const span) const {
        auto const escapes{std::ranges::any_of(path.lexically_normal(), [](auto const& part) {
            return part == "..";
        })};
        if (path.empty() || path.is_absolute() || path.has_root_path() || escapes) {
            fail(span, "generated output paths must remain inside the output root");
        }
    }

    auto parse_selection(Form const& form) const -> VariantSelection {
        if (head(form, "select") != "select") {
            fail(form.token.span, "expected SIMD lab selection");
        }

        VariantSelection result{.variant = VariantKind::out_of_place, .span = form.token.span};
        std::set<std::string> fields;
        bool has_variant{false};
        for (std::size_t index{1}; index < form.children.size(); ++index) {
            auto const& field{form.children[index]};
            auto const& field_name{head(field, "selection field")};
            if (!fields.insert(field_name).second) {
                fail(field.token.span, "duplicate selection field '" + field_name + "'");
            }
            if (field_name == "storage") {
                if (field.children.size() < 2) {
                    fail(field.token.span, "selection storage must not be empty");
                }
                for (std::size_t storage_index{1}; storage_index < field.children.size();
                     ++storage_index) {
                    auto const& value{
                        atom(field.children[storage_index], "expected 'array' or 'scalar'")};
                    if (value == "array") {
                        result.storage.push_back(StorageKind::array);
                    } else if (value == "scalar") {
                        result.storage.push_back(StorageKind::scalar);
                    } else {
                        fail(field.children[storage_index].token.span,
                             "unknown operand storage '" + value + "'");
                    }
                }
                continue;
            }

            require_size(field, 2, "single-value selection field");
            if (field_name == "operation") {
                result.operation = atom(field.children[1], "expected operation name");
            } else if (field_name == "type") {
                result.type = atom(field.children[1], "expected concrete type");
            } else if (field_name == "variant") {
                auto const& value{atom(field.children[1], "expected variant kind")};
                if (value == "out-of-place") {
                    result.variant = VariantKind::out_of_place;
                } else if (value == "in-place") {
                    result.variant = VariantKind::in_place;
                } else {
                    fail(field.children[1].token.span, "unknown variant kind '" + value + "'");
                }
                has_variant = true;
            } else {
                fail(field.token.span, "unknown selection field '" + field_name + "'");
            }
        }
        if (result.operation.empty() || result.type.empty() || result.storage.empty() ||
            !has_variant) {
            fail(form.token.span,
                 "select requires operation, type, storage, and variant");
        }
        return result;
    }

    auto parse_emission(Form const& form) const -> Emission {
        if (head(form, "emit") != "emit" || form.children.size() < 2) {
            fail(form.token.span,
                 "expected '(emit unreal|standard|unreal-avx2-lab|native-x86-simd-lab ...)'");
        }
        auto const& profile_name{atom(form.children[1], "expected emission profile")};
        Profile profile;
        if (profile_name == "unreal") {
            profile = Profile::unreal;
        } else if (profile_name == "standard") {
            profile = Profile::standard;
        } else if (profile_name == "unreal-avx2-lab") {
            profile = Profile::unreal_avx2_lab;
        } else if (profile_name == "native-x86-simd-lab") {
            profile = Profile::native_x86_simd_lab;
        } else {
            fail(form.children[1].token.span, "unknown emission profile '" + profile_name + "'");
        }

        Emission result{.profile = profile, .span = form.token.span};
        std::set<std::string> fields;
        for (std::size_t index{2}; index < form.children.size(); ++index) {
            auto const& field{form.children[index]};
            auto const& field_name{head(field, "emission field")};
            if (!fields.insert(field_name).second) {
                fail(field.token.span, "duplicate emission field '" + field_name + "'");
            }
            if (field_name == "select") {
                result.selection = parse_selection(field);
                continue;
            }
            require_size(field, 2, "single-value emission field");
            if (field_name == "header") {
                result.header = string_value(field.children[1], "expected header path string");
            } else if (field_name == "source") {
                result.source = string_value(field.children[1], "expected source path string");
            } else if (field_name == "avx512-source") {
                result.avx512_source =
                    string_value(field.children[1], "expected AVX-512 source path string");
            } else if (field_name == "dispatch-source") {
                result.dispatch_source =
                    string_value(field.children[1], "expected dispatch source path string");
            } else if (field_name == "tests") {
                result.tests = string_value(field.children[1], "expected test path string");
            } else if (field_name == "header-include") {
                result.header_include =
                    string_value(field.children[1], "expected header include string");
            } else if (field_name == "namespace") {
                result.cpp_namespace = atom(field.children[1], "expected C++ namespace");
                if (!is_qualified_identifier(result.cpp_namespace)) {
                    fail(field.children[1].token.span,
                         "namespace must be a qualified C++ identifier");
                }
            } else if (field_name == "export") {
                result.export_specifier = atom(field.children[1], "expected export specifier");
                if (!is_identifier(result.export_specifier)) {
                    fail(field.children[1].token.span, "export specifier must be an identifier");
                }
            } else {
                fail(field.token.span, "unknown emission field '" + field_name + "'");
            }
        }
        if (result.header.empty() || result.source.empty() || result.header_include.empty() ||
            result.cpp_namespace.empty()) {
            fail(form.token.span,
                 "emit requires header, source, header-include, and namespace");
        }
        if (profile == Profile::unreal && result.export_specifier.empty()) {
            fail(form.token.span, "unreal emission requires export");
        }
        if (profile == Profile::unreal && result.tests) {
            fail(form.token.span, "generated tests are supported only by the standard profile");
        }
        if (profile == Profile::standard && !result.export_specifier.empty()) {
            fail(form.token.span, "export is supported only by the unreal profile");
        }
        auto const is_simd_lab{profile == Profile::unreal_avx2_lab ||
                               profile == Profile::native_x86_simd_lab};
        if (!is_simd_lab && result.selection) {
            fail(form.token.span, "select is supported only by SIMD lab profiles");
        }
        if (is_simd_lab && !result.selection) {
            fail(form.token.span, "SIMD lab emission requires select");
        }
        if (is_simd_lab && (!result.export_specifier.empty() || result.tests)) {
            fail(form.token.span, "SIMD lab emissions do not support export or tests");
        }
        if (profile == Profile::native_x86_simd_lab &&
            (!result.avx512_source || !result.dispatch_source)) {
            fail(form.token.span,
                 "native-x86-simd-lab emission requires avx512-source and dispatch-source");
        }
        if (profile != Profile::native_x86_simd_lab &&
            (result.avx512_source || result.dispatch_source)) {
            fail(form.token.span,
                 "avx512-source and dispatch-source are supported only by native-x86-simd-lab");
        }
        validate_output_path(result.header, result.span);
        validate_output_path(result.source, result.span);
        if (result.avx512_source) {
            validate_output_path(*result.avx512_source, result.span);
        }
        if (result.dispatch_source) {
            validate_output_path(*result.dispatch_source, result.span);
        }
        if (result.tests) {
            validate_output_path(*result.tests, result.span);
        }
        return result;
    }

    auto parse_module(Form const& form) const -> KernelModule {
        if (head(form, "kernel-module") != "kernel-module" || form.children.size() < 2) {
            fail(form.token.span, "expected '(kernel-module name ...)'");
        }
        auto const& name{atom(form.children[1], "expected kernel module name")};
        if (!is_identifier(name)) {
            fail(form.children[1].token.span, "kernel module name must be an identifier");
        }
        KernelModule result{.name = name, .span = form.token.span};
        std::set<std::string> fields;
        std::set<Profile> profiles;
        std::set<std::string> type_sets;
        std::set<std::string> operation_names;
        for (std::size_t index{2}; index < form.children.size(); ++index) {
            auto const& field{form.children[index]};
            auto const& field_name{head(field, "kernel module field")};
            if (field_name == "type-set") {
                auto type_set{parse_type_set(field)};
                if (!type_sets.insert(type_set.name).second) {
                    fail(type_set.span, "duplicate type-set '" + type_set.name + "'");
                }
                result.type_sets.push_back(std::move(type_set));
                continue;
            }
            if (field_name == "map") {
                auto operation{parse_operation(field)};
                if (!operation_names.insert(operation.name).second) {
                    fail(operation.span, "duplicate map '" + operation.name + "'");
                }
                result.operations.push_back(std::move(operation));
                continue;
            }
            if (field_name == "emit") {
                auto emission{parse_emission(field)};
                if (!profiles.insert(emission.profile).second) {
                    fail(emission.span, "duplicate emission profile");
                }
                result.emissions.push_back(std::move(emission));
                continue;
            }
            if (!fields.insert(field_name).second) {
                fail(field.token.span, "duplicate kernel module field '" + field_name + "'");
            }
            fail(field.token.span, "unknown kernel module field '" + field_name + "'");
        }
        if (result.emissions.empty() || result.type_sets.empty() || result.operations.empty()) {
            fail(form.token.span,
                 "kernel-module requires emissions, type sets, and maps");
        }
        for (auto const& operation : result.operations) {
            auto const operation_types{
                std::ranges::find_if(result.type_sets, [&](auto const& type_set) {
                    return type_set.name == operation.type_set;
                })};
            if (operation_types == result.type_sets.end()) {
                fail(operation.span, "unknown type-set '" + operation.type_set + "'");
            }
            for (auto const& type : operation_types->types) {
                validate_expression_type(operation.expression, type);
            }
            for (auto const& variant : operation.variants) {
                if (variant.kind != VariantKind::in_place) {
                    continue;
                }
                auto const found{std::ranges::find_if(operation.operands, [&](auto const& operand) {
                    return operand.name == *variant.target;
                })};
                if (std::ranges::find(found->storage, StorageKind::array) == found->storage.end()) {
                    fail(variant.span, "in-place target must allow array storage");
                }
            }
        }
        for (auto const& emission : result.emissions) {
            if (!emission.selection) {
                continue;
            }
            auto const& selection{*emission.selection};
            auto const operation{std::ranges::find_if(result.operations, [&](auto const& item) {
                return item.name == selection.operation;
            })};
            if (operation == result.operations.end()) {
                fail(selection.span,
                     "selection references unknown operation '" + selection.operation + "'");
            }
            auto const type_set{std::ranges::find_if(result.type_sets, [&](auto const& item) {
                return item.name == operation->type_set;
            })};
            if (std::ranges::find(type_set->types, selection.type) == type_set->types.end()) {
                fail(selection.span,
                     "selection type '" + selection.type + "' is not supported by operation '" +
                         selection.operation + "'");
            }
            if (selection.storage.size() != operation->operands.size()) {
                fail(selection.span, "selection storage count must match the operation operands");
            }
            for (std::size_t index{}; index < selection.storage.size(); ++index) {
                if (std::ranges::find(operation->operands[index].storage,
                                      selection.storage[index]) ==
                    operation->operands[index].storage.end()) {
                    fail(selection.span,
                         "selection uses storage not supported by operand '" +
                             operation->operands[index].name + "'");
                }
            }
            auto const variant{std::ranges::find_if(operation->variants, [&](auto const& item) {
                return item.kind == selection.variant;
            })};
            if (variant == operation->variants.end()) {
                fail(selection.span, "selection variant is not generated by the operation");
            }
        }
        return result;
    }

    std::string path_;
    std::vector<Token> tokens_;
};

}

auto parse(std::string_view const path, std::vector<Token> tokens) -> Document {
    return Parser{path, std::move(tokens)}.parse();
}

}
