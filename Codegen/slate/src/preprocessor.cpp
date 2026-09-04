#include "preprocessor.h"

#include "lexer.h"
#include "manifest.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <utility>

namespace slate_codegen::detail {
namespace {

struct Form {
    Token token;
    std::vector<Form> children;
    Token closing;

    auto is_list() const -> bool { return token.kind == TokenKind::left_parenthesis; }

    auto head() const -> std::string_view {
        if (is_list() && !children.empty() && children.front().token.kind == TokenKind::atom) {
            return children.front().token.text;
        }
        return {};
    }
};

[[noreturn]] void fail(SourceSpan const& span, std::string const& message) {
    throw SourceError{span.path, span, message};
}

auto read_form(std::vector<Token> const& tokens, std::size_t& index) -> Form {
    auto const token{tokens[index++]};
    if (token.kind == TokenKind::right_parenthesis || token.kind == TokenKind::end) {
        fail(token.span, "expected an expression");
    }
    Form result{token, {}, token};
    if (result.is_list()) {
        while (tokens[index].kind != TokenKind::right_parenthesis) {
            if (tokens[index].kind == TokenKind::end) {
                fail(token.span, "expected ')' after expression");
            }
            result.children.push_back(read_form(tokens, index));
        }
        result.closing = tokens[index++];
    }
    return result;
}

auto location(SourceSpan const& span) -> std::string {
    return span.path + ":" + std::to_string(span.line) + ":" + std::to_string(span.column);
}

auto is_name(std::string const& name) -> bool {
    return !name.empty() && std::islower(static_cast<unsigned char>(name.front())) != 0 &&
           std::ranges::all_of(name, [](unsigned char c) {
               return std::isalnum(c) != 0 || c == '_' || c == '-';
           });
}

struct Macro {
    std::vector<std::string> parameters;
    Form body;
};

class Preprocessor {
  public:
    explicit Preprocessor(std::vector<std::filesystem::path> const& include_directories)
        : include_directories_{include_directories} {}

    auto run(std::filesystem::path const& input) -> std::vector<Token> {
        source_directory_ = std::filesystem::canonical(input).parent_path();
        auto forms{load(input, false, SourceSpan{})};
        std::vector<Token> result;
        for (auto const& form : forms) {
            flatten(expand(form), result);
        }
        result.push_back(Token{TokenKind::end, {}, SourceSpan{1, 1, input.generic_string(), {}}});
        return result;
    }

  private:
    auto resolve(Form const& form, std::filesystem::path const& including) const
        -> std::filesystem::path {
        if (form.children.size() != 2 || form.children[1].token.kind != TokenKind::string) {
            fail(form.token.span, "include requires one quoted relative path");
        }
        std::filesystem::path const requested{form.children[1].token.text};
        if (requested.empty() || requested.has_root_path()) {
            fail(form.token.span, "include requires a nonempty relative path");
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
            searched += "\n  " + candidate.lexically_normal().generic_string();
        }
        fail(form.token.span, "include not found: " + requested.generic_string() +
                                  "\nsearched:" + searched);
    }

    auto load(std::filesystem::path const& input, bool included, SourceSpan const& invocation)
        -> std::vector<Form> {
        auto const path{std::filesystem::canonical(input)};
        if (std::ranges::find(active_files_, path) != active_files_.end()) {
            std::string chain;
            for (auto const& active : active_files_) {
                chain += "\n  " + active.generic_string();
            }
            fail(invocation, "include cycle:" + chain + "\n  " + path.generic_string());
        }
        if (!loaded_files_.insert(path).second) {
            return {};
        }
        active_files_.push_back(path);
        auto tokens{lex(path.generic_string(), read_file(path))};
        for (auto& token : tokens) {
            token.span.path = path.lexically_relative(source_directory_).generic_string();
        }
        std::vector<Form> result;
        std::size_t index{};
        while (tokens[index].kind != TokenKind::end) {
            auto form{read_form(tokens, index)};
            if (form.head() == "include") {
                static_cast<void>(load(resolve(form, path), true, form.token.span));
            } else if (form.head() == "defmacro") {
                define(form);
            } else if (included) {
                fail(form.token.span, "included files may contain only include and defmacro declarations");
            } else {
                result.push_back(std::move(form));
            }
        }
        active_files_.pop_back();
        return result;
    }

    void define(Form const& form) {
        if (form.children.size() != 4 || form.children[1].token.kind != TokenKind::atom ||
            !form.children[2].is_list()) {
            fail(form.token.span, "expected (defmacro name (parameters...) expression)");
        }
        auto const& name{form.children[1].token.text};
        static std::set<std::string> const reserved{
            "include", "defmacro", "widget-class", "widget-library", "function", "params", "let", "vbox",
            "hbox", "auto", "fill", "assign", "existing", "call", "slot", "loc",
            "callback", "method", "uobject", "value", "factory"};
        if (!is_name(name) || reserved.contains(name)) {
            fail(form.token.span, "macro name must start with a lowercase letter and not name a built-in form");
        }
        Macro macro{{}, form.children[3]};
        std::set<std::string> parameters;
        for (auto const& parameter : form.children[2].children) {
            if (parameter.token.kind != TokenKind::atom || !is_name(parameter.token.text) ||
                !parameters.insert(parameter.token.text).second) {
                fail(parameter.token.span, "invalid or duplicate macro parameter");
            }
            macro.parameters.push_back(parameter.token.text);
        }
        validate_template(macro.body, parameters);
        if (auto const previous{macros_.find(name)}; previous != macros_.end()) {
            fail(form.token.span, "duplicate macro '" + name + "'; first defined at " +
                                      location(previous->second.body.token.span));
        }
        macros_.emplace(name, std::move(macro));
    }

    void validate_template(Form const& form, std::set<std::string> const& parameters) const {
        if (form.token.kind == TokenKind::atom && form.token.text.starts_with('$') &&
            !parameters.contains(form.token.text.substr(1))) {
            fail(form.token.span, "unknown macro parameter '" + form.token.text + "'");
        }
        for (auto const& child : form.children) {
            validate_template(child, parameters);
        }
    }

    auto substitute(Form const& form, std::map<std::string, Form> const& arguments,
                    std::string const& trace) const -> Form {
        if (form.token.kind == TokenKind::atom && form.token.text.starts_with('$')) {
            auto result{arguments.at(form.token.text.substr(1))};
            add_trace(result, trace);
            return result;
        }
        auto result{form};
        result.token.span.expansion += trace;
        result.closing.span.expansion += trace;
        for (auto& child : result.children) {
            child = substitute(child, arguments, trace);
        }
        return result;
    }

    static void add_trace(Form& form, std::string const& trace) {
        form.token.span.expansion += trace;
        form.closing.span.expansion += trace;
        for (auto& child : form.children) {
            add_trace(child, trace);
        }
    }

    auto expand(Form const& form) -> Form {
        if (++expanded_forms_ > 100000) {
            fail(form.token.span, "macro expansion exceeds 100000 expressions");
        }
        if (form.head() == "include" || form.head() == "defmacro") {
            fail(form.token.span, "include and defmacro must be top-level source declarations");
        }
        auto const found{macros_.find(std::string{form.head()})};
        if (found != macros_.end()) {
            auto const& [name, macro]{*found};
            if (std::ranges::find(active_macros_, name) != active_macros_.end()) {
                fail(form.token.span, "recursive macro expansion of '" + name + "'");
            }
            if (form.children.size() != macro.parameters.size() + 1) {
                fail(form.token.span, "macro '" + name + "' expects " +
                                          std::to_string(macro.parameters.size()) + " arguments");
            }
            std::map<std::string, Form> arguments;
            auto const count{macro.parameters.size()};
            for (std::size_t i{}; i < count; ++i) {
                arguments.emplace(macro.parameters[i], expand(form.children[i + 1]));
            }
            auto const trace{"\n  macro '" + name + "' defined at " + location(macro.body.token.span) +
                             "\n  expanded at " + location(form.token.span) + form.token.span.expansion};
            active_macros_.push_back(name);
            auto result{expand(substitute(macro.body, arguments, trace))};
            active_macros_.pop_back();
            return result;
        }
        if (form.token.kind == TokenKind::atom && form.token.text.starts_with('$')) {
            fail(form.token.span, "macro parameter outside a template: " + form.token.text);
        }
        auto result{form};
        for (auto& child : result.children) {
            child = expand(child);
        }
        return result;
    }

    static void flatten(Form const& form, std::vector<Token>& tokens) {
        tokens.push_back(form.token);
        for (auto const& child : form.children) {
            flatten(child, tokens);
        }
        if (form.is_list()) {
            tokens.push_back(form.closing);
        }
    }

    std::vector<std::filesystem::path> const& include_directories_;
    std::filesystem::path source_directory_;
    std::set<std::filesystem::path> loaded_files_;
    std::vector<std::filesystem::path> active_files_;
    std::map<std::string, Macro> macros_;
    std::vector<std::string> active_macros_;
    std::size_t expanded_forms_{};
};

}

auto preprocess(std::filesystem::path const& input,
                std::vector<std::filesystem::path> const& include_directories)
    -> std::vector<Token> {
    return Preprocessor{include_directories}.run(input);
}

}
