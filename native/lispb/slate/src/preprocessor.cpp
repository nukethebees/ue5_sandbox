#include "preprocessor.h"

#include <codegen/cpp_string.h>
#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/source_span.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <codegen/sexpr/token_kind.h>
#include <lispb/diagnostics.h>
#include <lispb/indices.h>
#include <lispb/project_loader.h>
#include <lispb/strings.h>
#include <slate_codegen/indices.h>
#include <slate_codegen/macros.h>
#include <slate_codegen/range.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iterator>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace slate_codegen::detail {
namespace {

using lispb::StringId;
using lispb::codegen::sexpr::FormId;
using lispb::codegen::sexpr::HandleRange;
using lispb::codegen::sexpr::SourceSpan;
using lispb::codegen::sexpr::Syntax;
using lispb::codegen::sexpr::TokenKind;

void format_form(Syntax const& syntax,
                 FormId const form,
                 std::size_t const indent,
                 std::string& output) {
    if (!syntax.is_list(form)) {
        auto const token{syntax.token(form)};
        if (syntax.kind(token) == TokenKind::raw_literal) {
            output += '#';
            output += syntax.tag(token);
            output += '{';
            output += syntax.text(token);
            output += '}';
            output += syntax.tag(token);
            output += '#';
        } else if (syntax.kind(token) == TokenKind::keyword) {
            output += ':';
            output += syntax.text(token);
        } else if (syntax.kind(token) == TokenKind::string) {
            output += '"';
            output += lispb::codegen::escape_cpp_string(syntax.text(token));
            output += '"';
        } else {
            output += syntax.text(token);
        }
        return;
    }
    output += '(';
    auto const count{syntax.children(form).size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const child{syntax.children(form)[index]};
        if (index != 0) {
            auto const previous{syntax.children(form)[index - 1]};
            if (syntax.kind(previous) != TokenKind::keyword &&
                (syntax.is_list(child) || syntax.is_list(previous) ||
                 syntax.kind(child) == TokenKind::keyword)) {
                output += '\n';
                output.append(indent + 2, ' ');
            } else {
                output += ' ';
            }
        }
        format_form(syntax, child, indent + 2, output);
    }
    output += ')';
}

auto is_name(std::string_view const name) -> bool {
    return !name.empty() && std::islower(static_cast<unsigned char>(name.front())) != 0 &&
           std::ranges::all_of(name, [](unsigned char const character) {
               return std::isalnum(character) != 0 || character == '_' || character == '-';
           });
}

class Preprocessor {
  public:
    Preprocessor(Syntax& syntax, std::vector<std::filesystem::path> const& include_directories)
        : syntax_{syntax}
        , include_directories_{include_directories} {}

    auto run(std::filesystem::path const& input) -> PreprocessedSource {
        source_directory_ = std::filesystem::canonical(input).parent_path();
        auto const forms{load(input, false, {})};
        std::vector<FormId> expanded;
        expanded.reserve(forms.size());
        for (auto const form : forms) {
            expanded.push_back(expand(form, {}));
        }
        return {syntax_.range(expanded), {loaded_files_.begin(), loaded_files_.end()}};
    }
  private:
    struct Argument {
        StringId parameter;
        FormId value;
    };

    [[noreturn]] void fail(SourceSpan const span, std::string const& message) const {
        syntax_.sources().diagnostics().fail(syntax_.sources(), span, message);
    }
    auto location(SourceSpan const span) const -> std::string {
        auto const location{syntax_.sources().location(span)};
        return std::format("{}:{}:{}", location.path, location.line, location.column);
    }
    auto resolve(FormId const form, std::filesystem::path const& including) const
        -> std::filesystem::path {
        if (syntax_.children(form).size() != 2 ||
            syntax_.kind(syntax_.children(form)[1]) != TokenKind::string) {
            fail(syntax_.token_span(form), "include requires one quoted relative path");
        }
        std::filesystem::path const requested{syntax_.text(syntax_.children(form)[1])};
        if (requested.empty() || requested.has_root_path()) {
            fail(syntax_.token_span(form), "include requires a nonempty relative path");
        }
        std::vector<std::filesystem::path> candidates{including.parent_path() / requested};
        for (auto const& directory : include_directories_) {
            candidates.push_back(directory / requested);
        }
        std::string searched;
        for (auto const& candidate : candidates) {
            if (std::filesystem::is_regular_file(candidate)) {
                return candidate;
            }
            std::format_to(std::back_inserter(searched),
                           "\n  {}",
                           candidate.lexically_normal().generic_string());
        }
        fail(syntax_.token_span(form),
             std::format(
                 "include not found: {}\nsearched:{}", requested.generic_string(), searched));
    }

    auto load(std::filesystem::path const& input, bool const included, SourceSpan const invocation)
        -> std::vector<FormId> {
        auto const path{std::filesystem::canonical(input)};
        if (std::ranges::find(active_files_, path) != active_files_.end()) {
            std::string chain;
            for (auto const& active : active_files_) {
                std::format_to(std::back_inserter(chain), "\n  {}", active.generic_string());
            }
            fail(invocation, std::format("include cycle:{}\n  {}", chain, path.generic_string()));
        }
        if (!loaded_files_.insert(path).second) {
            return {};
        }
        active_files_.push_back(path);
        auto const file{syntax_.sources().load(
            path, path.lexically_relative(source_directory_).generic_string())};
        auto const forms{lispb::codegen::sexpr::read_forms(syntax_, file)};
        std::vector<FormId> result;
        for (std::uint32_t index{}; index < forms.count; ++index) {
            auto const form{syntax_.forms(forms)[index]};
            if (syntax_.head(form) == "include") {
                static_cast<void>(load(resolve(form, path), true, syntax_.token_span(form)));
            } else if (syntax_.head(form) == "defmacro") {
                define(form);
            } else if (included) {
                fail(syntax_.token_span(form),
                     "included files may contain only include and defmacro declarations");
            } else {
                result.push_back(form);
            }
        }
        active_files_.pop_back();
        return result;
    }

    void define(FormId const id) {
        auto form{id};
        if (syntax_.children(form).size() != 4 ||
            syntax_.kind(syntax_.children(form)[1]) != TokenKind::atom ||
            !syntax_.is_list(syntax_.children(form)[2])) {
            fail(syntax_.token_span(form), "expected (defmacro name (parameters...) expression)");
        }
        auto const name{syntax_.identifier(syntax_.children(form)[1])};
        auto const spelling{syntax_.text(syntax_.children(form)[1])};
        constexpr std::string_view reserved[]{
            "include", "defmacro", "widget-class", "widget-library", "function",
            "params",  "let",      "vbox",         "hbox",           "auto",
            "fill",    "assign",   "existing",     "call",           "slot",
            "loc",     "callback", "method",       "uobject",        "value",
            "factory"};
        if (!is_name(spelling) || std::ranges::find(reserved, spelling) != std::end(reserved)) {
            fail(syntax_.token_span(form),
                 "macro name must start with a lowercase letter and not name a built-in form");
        }
        auto const macro{MacroId{static_cast<std::uint32_t>(macros_.bodies.size())}};
        auto const body{syntax_.children(form)[3]};
        auto const parameters_begin{static_cast<std::uint32_t>(macros_.parameter_names.size())};
        std::set<StringId> parameters;
        auto const parameter_count{syntax_.children(syntax_.children(form)[2]).size()};
        for (std::size_t index{}; index < parameter_count; ++index) {
            auto const parameter{syntax_.children(syntax_.children(id)[2])[index]};
            if (syntax_.kind(parameter) != TokenKind::atom || !is_name(syntax_.text(parameter)) ||
                !parameters.insert(syntax_.identifier(parameter)).second) {
                fail(syntax_.token_span(parameter), "invalid or duplicate macro parameter");
            }
            // Intern the placeholder once; substitution resolves identifier handles directly.
            auto const placeholder{
                syntax_.strings().intern("$" + std::string{syntax_.text(parameter)})};
            macros_.parameter_names.push_back(placeholder);
        }
        Range const parameter_range{parameters_begin, static_cast<std::uint32_t>(parameter_count)};
        validate_template(body, parameter_range);
        if (auto const previous{macro_names_.find(name)}; previous != macro_names_.end()) {
            fail(syntax_.token_span(id),
                 std::format("duplicate macro '{}'; first defined at {}",
                             syntax_.strings().view(name),
                             location(syntax_.token_span(macros_.bodies[previous->second.value]))));
        }
        macros_.parameters.push_back(parameter_range);
        macros_.bodies.push_back(body);
        macro_names_.emplace(name, macro);
    }

    void validate_template(FormId const form, Range const parameters) const {
        if (syntax_.kind(form) == TokenKind::atom && syntax_.text(form).starts_with('$')) {
            auto const end{parameters.begin + parameters.count};
            bool found{};
            for (auto index{parameters.begin}; index < end; ++index) {
                found = found || macros_.parameter_names[index] == syntax_.identifier(form);
            }
            if (!found) {
                fail(syntax_.token_span(form),
                     std::format("unknown macro parameter '{}'", syntax_.text(form)));
            }
        }
        for (auto const child : syntax_.children(form)) {
            validate_template(child, parameters);
        }
    }

    auto expand(FormId const form,
                std::span<Argument const> const arguments,
                lispb::codegen::sexpr::ExpansionId const expansion = {}) -> FormId {
        auto span{syntax_.token_span(form)};
        if (expansion.value != 0) {
            span.expansion = expansion;
        }
        if (++expanded_forms_ > 100000) {
            fail(span, "macro expansion exceeds 100000 expressions");
        }
        if (syntax_.kind(form) == TokenKind::atom && syntax_.text(form).starts_with('$')) {
            for (auto const argument : arguments) {
                if (argument.parameter == syntax_.identifier(form)) {
                    return argument.value;
                }
            }
            fail(span, std::format("macro parameter outside a template: {}", syntax_.text(form)));
        }
        if (syntax_.head(form) == "include" || syntax_.head(form) == "defmacro") {
            fail(span, "include and defmacro must be top-level source declarations");
        }
        auto head{syntax_.is_list(form) && !syntax_.children(form).empty()
                      ? syntax_.identifier(syntax_.child(form, 0))
                      : StringId{}};
        for (auto const argument : arguments) {
            if (argument.parameter == head) {
                head = syntax_.identifier(argument.value);
                break;
            }
        }
        auto const found{macro_names_.find(head)};
        if (found != macro_names_.end()) {
            auto const name{found->first};
            auto const macro{found->second};
            if (std::ranges::find(active_macros_, macro) != active_macros_.end()) {
                fail(
                    span,
                    std::format("recursive macro expansion of '{}'", syntax_.strings().view(name)));
            }
            auto const parameters{macros_.parameters[macro.value]};
            auto const body{macros_.bodies[macro.value]};
            if (syntax_.children(form).size() != parameters.count + 1) {
                fail(span,
                     std::format("macro '{}' expects {} arguments",
                                 syntax_.strings().view(name),
                                 parameters.count));
            }
            auto const trace{syntax_.sources().add_expansion(name, span, syntax_.token_span(body))};
            std::vector<Argument> bindings;
            bindings.reserve(parameters.count);
            for (std::uint32_t index{}; index < parameters.count; ++index) {
                bindings.push_back({macros_.parameter_names[parameters.begin + index],
                                    expand(syntax_.child(form, index + 1), arguments, trace)});
            }
            active_macros_.push_back(macro);
            auto const result{expand(body, bindings, trace)};
            active_macros_.pop_back();
            return result;
        }
        if (!syntax_.is_list(form)) {
            return expansion.value == 0 ? form : syntax_.add_leaf(syntax_.token(form), expansion);
        }
        auto const count{syntax_.children(form).size()};
        std::vector<FormId> children;
        children.reserve(count);
        bool changed{expansion.value != 0};
        for (std::size_t index{}; index < count; ++index) {
            auto const child{syntax_.child(form, index)};
            auto const expanded{expand(child, arguments, expansion)};
            changed = changed || expanded != child;
            children.push_back(expanded);
        }
        if (!changed) {
            return form;
        }
        auto closing{syntax_.closing_span(form)};
        closing.expansion = span.expansion;
        return syntax_.add_list(syntax_.token(form), children, closing);
    }

    Syntax& syntax_;
    std::vector<std::filesystem::path> const& include_directories_;
    std::filesystem::path source_directory_;
    std::set<std::filesystem::path> loaded_files_;
    std::vector<std::filesystem::path> active_files_;
    Macros macros_;
    std::map<StringId, MacroId> macro_names_;
    std::vector<MacroId> active_macros_;
    std::size_t expanded_forms_{};
};

}

auto preprocess(Syntax& syntax,
                std::filesystem::path const& input,
                std::vector<std::filesystem::path> const& include_directories)
    -> PreprocessedSource {
    return Preprocessor{syntax, include_directories}.run(input);
}

auto format_expansion(Syntax const& syntax, HandleRange const forms) -> std::string {
    std::string result;
    for (auto const form : syntax.forms(forms)) {
        if (!result.empty()) {
            result += '\n';
        }
        format_form(syntax, form, 0, result);
        result += '\n';
    }
    return result;
}

}
