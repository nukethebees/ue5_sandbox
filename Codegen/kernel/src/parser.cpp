#include "parser.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
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
            for (auto const& path : {module.header.generic_string(), module.source.generic_string()}) {
                if (!output_paths.insert(path).second) {
                    fail(module.span, "duplicate generated output path '" + path + "'");
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

    static auto is_literal(std::string const& value) -> bool {
        if (value.empty()) {
            return false;
        }
        char* end{};
        auto const parsed{std::strtod(value.c_str(), &end)};
        return end == value.c_str() + value.size() && std::isfinite(parsed);
    }

    auto parse_expression(Form const& form) const -> Expression {
        if (!form.is_list()) {
            auto const& value{atom(form, "expected operand reference or numeric literal")};
            return Expression{is_literal(value) ? ExpressionKind::literal
                                                : ExpressionKind::reference,
                              value,
                              {},
                              form.token.span};
        }
        require_size(form, 3, "binary expression '(operator lhs rhs)'");
        auto const& operation{atom(form.children[0], "expected expression operator")};
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
            if (type != "int32" && type != "float" && type != "double") {
                fail(form.children[index].token.span, "unsupported concrete type '" + type + "'");
            }
            if (!types.insert(type).second) {
                fail(form.children[index].token.span, "duplicate concrete type '" + type + "'");
            }
            result.types.push_back(type);
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
            if (!fields.insert(field_name).second) {
                fail(field.token.span, "duplicate kernel module field '" + field_name + "'");
            }
            require_size(field, 2, "single-value kernel module field");
            if (field_name == "header") {
                result.header = string_value(field.children[1], "expected header path string");
            } else if (field_name == "source") {
                result.source = string_value(field.children[1], "expected source path string");
            } else if (field_name == "header-include") {
                result.header_include =
                    string_value(field.children[1], "expected header include string");
            } else if (field_name == "namespace") {
                result.cpp_namespace = atom(field.children[1], "expected C++ namespace");
                if (!is_qualified_identifier(result.cpp_namespace)) {
                    fail(field.children[1].token.span, "namespace must be a qualified C++ identifier");
                }
            } else if (field_name == "export") {
                result.export_specifier = atom(field.children[1], "expected export specifier");
                if (!is_identifier(result.export_specifier)) {
                    fail(field.children[1].token.span, "export specifier must be an identifier");
                }
            } else {
                fail(field.token.span, "unknown kernel module field '" + field_name + "'");
            }
        }
        if (result.header.empty() || result.source.empty() || result.header_include.empty() ||
            result.cpp_namespace.empty() || result.export_specifier.empty() ||
            result.type_sets.empty() || result.operations.empty()) {
            fail(form.token.span,
                 "kernel-module requires header, source, header-include, namespace, export, "
                 "type sets, and maps");
        }
        for (auto const& path : {result.header, result.source}) {
            auto const escapes{std::ranges::any_of(path.lexically_normal(), [](auto const& part) {
                return part == "..";
            })};
            if (path.empty() || path.is_absolute() || path.has_root_path() || escapes) {
                fail(form.token.span, "generated output paths must remain inside the output root");
            }
        }
        for (auto const& operation : result.operations) {
            if (!type_sets.contains(operation.type_set)) {
                fail(operation.span, "unknown type-set '" + operation.type_set + "'");
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
