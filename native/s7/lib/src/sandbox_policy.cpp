#include "sandbox_policy.h"

#include "s7_sandbox.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace ioj::s7::detail {
inline constexpr std::string_view explicitly_disabled_bindings[]{
    "autoload",
    "call-with-input-file",
    "call-with-output-file",
    "delete-file",
    "directory->list",
    "directory?",
    "emergency-exit",
    "exit",
    "file-exists?",
    "file-mtime",
    "getenv",
    "load",
    "open-input-file",
    "open-output-file",
    "require",
    "system",
    "with-input-from-file",
    "with-output-to-file",
};

inline constexpr std::string_view allowed_bindings[]{
    "*",
    "+",
    "-",
    "/",
    "<",
    "<=",
    "=",
    ">",
    ">=",
    "abs",
    "acos",
    "acosh",
    "and",
    "<list*>",
    "append",
    "apply",
    "apply-values",
    "aritable?",
    "arity",
    "asin",
    "asinh",
    "ash",
    "assoc",
    "assq",
    "assv",
    "atan",
    "atanh",
    "begin",
    "boolean?",
    "byte?",
    "byte-vector",
    "byte-vector?",
    "byte-vector-ref",
    "byte-vector-set!",
    "caaaar",
    "caaadr",
    "caaar",
    "caadar",
    "caaddr",
    "caadr",
    "caar",
    "cadaar",
    "cadadr",
    "cadar",
    "caddar",
    "cadddr",
    "caddr",
    "cadr",
    "call-with-current-continuation",
    "call-with-exit",
    "call-with-values",
    "call/cc",
    "car",
    "case",
    "catch",
    "cdaaar",
    "cdaadr",
    "cdaar",
    "cdadar",
    "cdaddr",
    "cdadr",
    "cdar",
    "cddaar",
    "cddadr",
    "cddar",
    "cdddar",
    "cddddr",
    "cdddr",
    "cddr",
    "cdr",
    "ceiling",
    "char->integer",
    "char-alphabetic?",
    "char-ci<=?",
    "char-ci<?",
    "char-ci=?",
    "char-ci>=?",
    "char-ci>?",
    "char-downcase",
    "char-lower-case?",
    "char-numeric?",
    "char-position",
    "char-upcase",
    "char-upper-case?",
    "char-whitespace?",
    "char<=?",
    "char<?",
    "char=?",
    "char>=?",
    "char>?",
    "char?",
    "cond",
    "cond-expand",
    "cons",
    "copy",
    "cos",
    "cosh",
    "cyclic-sequences",
    "define",
    "define*",
    "define-constant",
    "define-expansion",
    "define-macro",
    "define-macro*",
    "delay",
    "delay-force",
    "denominator",
    "do",
    "dynamic-wind",
    "else",
    "eq?",
    "equal?",
    "equivalent?",
    "error",
    "eval",
    "eval-string",
    "even?",
    "exact->inexact",
    "exact?",
    "exp",
    "expt",
    "finite?",
    "fill!",
    "float-vector",
    "float-vector?",
    "float-vector-ref",
    "float-vector-set!",
    "floor",
    "for-each",
    "force",
    "format",
    "gcd",
    "gensym",
    "hash-table",
    "hash-table-entries",
    "hash-table-ref",
    "hash-table-set!",
    "hash-table?",
    "if",
    "immutable?",
    "inexact->exact",
    "inexact?",
    "infinite?",
    "int-vector",
    "int-vector?",
    "int-vector-ref",
    "int-vector-set!",
    "integer->char",
    "integer-decode-float",
    "integer?",
    "keyword->symbol",
    "keyword?",
    "lambda",
    "lambda*",
    "lcm",
    "length",
    "let",
    "let*",
    "letrec",
    "letrec*",
    "list",
    "list-ref",
    "list-set!",
    "list-tail",
    "list-values",
    "list?",
    "log",
    "logand",
    "logbit?",
    "logeqv",
    "logior",
    "lognand",
    "lognor",
    "lognot",
    "logxor",
    "macro?",
    "macroexpand",
    "magnitude",
    "make-byte-vector",
    "make-float-vector",
    "make-hash-table",
    "make-int-vector",
    "make-list",
    "make-promise",
    "make-string",
    "make-vector",
    "map",
    "max",
    "member",
    "memq",
    "memv",
    "min",
    "modulo",
    "nan?",
    "negative?",
    "not",
    "null?",
    "number->string",
    "number?",
    "numerator",
    "object->string",
    "odd?",
    "or",
    "pair?",
    "pi",
    "positive?",
    "procedure?",
    "promise?",
    "quasiquote",
    "quotient",
    "rational?",
    "rationalize",
    "real?",
    "remainder",
    "reverse",
    "reverse!",
    "round",
    "set!",
    "set-car!",
    "set-cdr!",
    "signature",
    "sin",
    "sinh",
    "sort!",
    "sqrt",
    "string",
    "string->keyword",
    "string->number",
    "string->symbol",
    "string-append",
    "string-ci<=?",
    "string-ci<?",
    "string-ci=?",
    "string-ci>=?",
    "string-ci>?",
    "string-copy",
    "string-downcase",
    "string-fill!",
    "string-length",
    "string-position",
    "string-ref",
    "string-set!",
    "string-upcase",
    "string<=?",
    "string<?",
    "string=?",
    "string>=?",
    "string>?",
    "string?",
    "substring",
    "symbol",
    "symbol->keyword",
    "symbol->string",
    "symbol?",
    "tan",
    "tanh",
    "throw",
    "tree-count",
    "tree-cyclic?",
    "tree-leaves",
    "tree-memq",
    "truncate",
    "type-of",
    "unless",
    "values",
    "vector",
    "vector-append",
    "vector-dimensions",
    "vector-fill!",
    "vector-length",
    "vector-rank",
    "vector-ref",
    "vector-set!",
    "vector?",
    "when",
    "zero?",
    source_variable_name,
    "arguments",
    "consumer",
    "control",
    "destination",
    "host-format",
    "info",
    "producer",
    "type",
};

[[nodiscard]] auto is_allowed_binding(std::string_view const name) -> bool {
    return std::ranges::find(allowed_bindings, name) != std::end(allowed_bindings);
}

auto disabled_operation(s7_scheme* const scheme, s7_pointer) -> s7_pointer {
    return s7_error(
        scheme,
        s7_make_symbol(scheme, "permission-error"),
        s7_list(scheme,
                1,
                s7_make_string(scheme, "This operation is disabled by the embedded s7 runtime.")));
}

struct SandboxBindings {
    s7_scheme* scheme{};
    s7_pointer disabled{};
};

auto revoke_unapproved_binding(char const* const name, void* const raw_context) -> bool {
    auto& context{*static_cast<SandboxBindings*>(raw_context)};
    std::string_view const binding_name{name};
    if (is_allowed_binding(binding_name)) {
        return false;
    }

    auto const symbol{s7_symbol_table_find_name(context.scheme, name)};
    if (symbol != nullptr &&
        s7_symbol_local_value(context.scheme, symbol, s7_rootlet(context.scheme)) !=
            s7_undefined(context.scheme)) {
        s7_define(context.scheme, s7_rootlet(context.scheme), symbol, context.disabled);
        s7_symbol_force_set_initial_value(context.scheme, symbol, context.disabled);
    }
    return false;
}

void restrict_global_environment(s7_scheme& scheme, s7_pointer const disabled) {
    SandboxBindings bindings{.scheme = &scheme, .disabled = disabled};
    s7_for_each_symbol(&scheme, revoke_unapproved_binding, &bindings);

    for (auto const name : explicitly_disabled_bindings) {
        auto const owned_name{std::string{name}};
        auto const symbol{s7_make_symbol(&scheme, owned_name.c_str())};
        s7_define(&scheme, s7_rootlet(&scheme), symbol, disabled);
        s7_symbol_force_set_initial_value(&scheme, symbol, disabled);
    }
}

auto prepare_safe_format(s7_scheme* const scheme) -> s7_pointer {
    return s7_eval_c_string(scheme, R"(
        (let ((host-format format))
          (lambda (destination control . arguments)
            (if (eq? destination #f)
                (apply host-format destination control arguments)
                (error 'permission-error
                       "format can only produce a string in the embedded s7 runtime.")))))");
}

auto prepare_disabled_operation(s7_scheme* const scheme) -> s7_pointer {
    return s7_make_function(scheme,
                            "#<disabled-operation>",
                            disabled_operation,
                            0,
                            0,
                            true,
                            "Disabled by the embedded s7 runtime.");
}

void install_binding(s7_scheme* const scheme, char const* const name, s7_pointer const value) {
    auto const symbol{s7_make_symbol(scheme, name)};
    s7_define(scheme, s7_rootlet(scheme), symbol, value);
    s7_symbol_force_set_initial_value(scheme, symbol, value);
}
}
