#include <native/s7/interpreter.h>

#include "s7.h"
#include "s7_sandbox.h"

#if !defined(_WIN32)
#error The sandboxed s7 loader currently requires Windows.
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ml::s7 {
namespace {
constexpr char source_variable_name[]{"*sandbox-s7-source*"};
constexpr char permission_error_name[]{"permission-error"};

constexpr std::string_view explicitly_disabled_bindings[]{
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

constexpr std::string_view allowed_bindings[]{
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

auto sandbox_error(s7_scheme* const scheme, char const* const type, std::string const& message)
    -> s7_pointer {
    return s7_error(scheme,
                    s7_make_symbol(scheme, type),
                    s7_list(scheme, 1, s7_make_string(scheme, message.c_str())));
}

auto load_failure(s7_scheme* const scheme, std::string const& message) -> s7_pointer {
    return s7_cons(scheme, s7_make_integer(scheme, 0), s7_make_string(scheme, message.c_str()));
}

auto disabled_operation(s7_scheme* const scheme, s7_pointer) -> s7_pointer {
    return sandbox_error(
        scheme, permission_error_name, "This operation is disabled by the embedded s7 runtime.");
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

auto object_to_string(s7_scheme* const scheme, s7_pointer const value) -> std::string {
    char* const text{s7_object_to_c_string(scheme, value)};
    if (text == nullptr) {
        return {};
    }

    std::string result{text};
    std::free(text);
    return result;
}

struct CanonicalPath {
    std::wstring wide_path;
    std::string narrow_path;
    std::size_t file_size{};
};

[[nodiscard]] auto utf8_to_wide(std::string_view const value) -> std::optional<std::wstring> {
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }
    auto const input_size{static_cast<int>(value.size())};
    auto const output_size{
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), input_size, nullptr, 0)};
    if (output_size <= 0) {
        return std::nullopt;
    }

    std::wstring result(static_cast<std::size_t>(output_size), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), input_size, result.data(), output_size) !=
        output_size) {
        return std::nullopt;
    }
    return result;
}

[[nodiscard]] auto wide_to_utf8(std::wstring_view const value) -> std::optional<std::string> {
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }
    auto const input_size{static_cast<int>(value.size())};
    auto const output_size{WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), input_size, nullptr, 0, nullptr, nullptr)};
    if (output_size <= 0) {
        return std::nullopt;
    }

    std::string result(static_cast<std::size_t>(output_size), '\0');
    if (WideCharToMultiByte(CP_UTF8,
                            WC_ERR_INVALID_CHARS,
                            value.data(),
                            input_size,
                            result.data(),
                            output_size,
                            nullptr,
                            nullptr) != output_size) {
        return std::nullopt;
    }
    return result;
}

[[nodiscard]] auto strip_extended_path_prefix(std::wstring path) -> std::wstring {
    constexpr std::wstring_view unc_prefix{LR"(\\?\UNC\)"};
    constexpr std::wstring_view local_prefix{LR"(\\?\)"};
    if (path.starts_with(unc_prefix)) {
        path.erase(0, unc_prefix.size());
        path.insert(path.begin(), 2, L'\\');
    } else if (path.starts_with(local_prefix)) {
        path.erase(0, local_prefix.size());
    }
    return path;
}

[[nodiscard]] auto canonical_path(std::wstring const& path, bool const require_directory)
    -> std::optional<CanonicalPath> {
    auto const handle{CreateFileW(path.c_str(),
                                  FILE_READ_ATTRIBUTES,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr,
                                  OPEN_EXISTING,
                                  require_directory ? FILE_FLAG_BACKUP_SEMANTICS : 0,
                                  nullptr)};
    if (handle == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }

    BY_HANDLE_FILE_INFORMATION file_information{};
    auto const has_information{GetFileInformationByHandle(handle, &file_information) != 0};
    auto const is_directory{(file_information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0};
    auto const required_size{
        GetFinalPathNameByHandleW(handle, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS)};
    if (!has_information || is_directory != require_directory || required_size == 0) {
        CloseHandle(handle);
        return std::nullopt;
    }

    std::wstring resolved(static_cast<std::size_t>(required_size), L'\0');
    auto const copied{GetFinalPathNameByHandleW(
        handle, resolved.data(), required_size, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS)};
    CloseHandle(handle);
    if (copied == 0 || copied >= required_size) {
        return std::nullopt;
    }
    resolved.resize(copied);
    resolved = strip_extended_path_prefix(std::move(resolved));

    auto const narrow{wide_to_utf8(resolved)};
    auto const size{(static_cast<std::uint64_t>(file_information.nFileSizeHigh) << 32U) |
                    file_information.nFileSizeLow};
    if (!narrow.has_value() || size > std::numeric_limits<std::size_t>::max()) {
        return std::nullopt;
    }
    return CanonicalPath{
        .wide_path = std::move(resolved),
        .narrow_path = std::move(*narrow),
        .file_size = static_cast<std::size_t>(size),
    };
}

[[nodiscard]] auto path_key(std::string key) -> std::string {
    std::ranges::transform(key, key.begin(), [](unsigned char const character) {
        return static_cast<char>(std::tolower(character));
    });
    return key;
}

[[nodiscard]] auto is_within_root(std::wstring_view const path, std::wstring_view const root)
    -> bool {
    if (path.size() <= root.size() || path[root.size()] != L'\\') {
        return false;
    }
    return CompareStringOrdinal(path.data(),
                                static_cast<int>(root.size()),
                                root.data(),
                                static_cast<int>(root.size()),
                                true) == CSTR_EQUAL;
}
}

class Interpreter::Impl final {
  public:
    explicit Impl(InterpreterOptions options)
        : options_{std::move(options)} {
        scheme_ = s7_init();
        if (scheme_ == nullptr) {
            std::abort();
        }

        source_symbol_ = s7_make_symbol(scheme_, source_variable_name);
        s7_define_variable(scheme_, source_variable_name, s7_make_string(scheme_, ""));

        evaluation_body_ = s7_eval_c_string(
            scheme_, "(lambda () (cons #t (eval-string *sandbox-s7-source* (rootlet))))");
        error_handler_ =
            s7_eval_c_string(scheme_, "(lambda (type info) (cons #f (apply format #f info)))");
        safe_format_ = s7_eval_c_string(
            scheme_,
            "(let ((host-format format))"
            "  (lambda (destination control . arguments)"
            "    (if (eq? destination #f)"
            "        (apply host-format destination control arguments)"
            "        (error 'permission-error"
            "               \"format can only produce a string in the embedded s7 runtime.\"))))");
        loader_factory_ = s7_eval_c_string(
            scheme_,
            "(let ((host-load load)"
            "      (host-format format)"
            "      (sandbox-root (rootlet))"
            "      (no-value (if #f #f)))"
            "  (lambda (resolve complete)"
            "    (lambda (path)"
            "      (let* ((resolution (resolve path))"
            "             (status (car resolution)))"
            "        (cond ((= status 0) (error 'load-script-error (cdr resolution)))"
            "              ((= status 1) no-value)"
            "              (else"
            "               (catch #t"
            "                 (lambda ()"
            "                   (host-load (cdr resolution) sandbox-root)"
            "                   (complete))"
            "                 (lambda (type info)"
            "                   (error type \"~A: ~A\""
            "                          (cdr resolution)"
            "                          (apply host-format #f info))))))))))");
        s7_gc_protect(scheme_, evaluation_body_);
        s7_gc_protect(scheme_, error_handler_);
        s7_gc_protect(scheme_, safe_format_);
        s7_gc_protect(scheme_, loader_factory_);

        auto const disabled{s7_make_function(scheme_,
                                             "#<disabled-operation>",
                                             disabled_operation,
                                             0,
                                             0,
                                             true,
                                             "Disabled by the embedded s7 runtime.")};
        s7_gc_protect(scheme_, disabled);
        restrict_global_environment(*scheme_, disabled);

        auto const format_symbol{s7_make_symbol(scheme_, "format")};
        s7_define(scheme_, s7_rootlet(scheme_), format_symbol, safe_format_);
        s7_symbol_force_set_initial_value(scheme_, format_symbol, safe_format_);

        {
            std::scoped_lock const lock{instances_mutex_};
            instances_.emplace(scheme_, this);
        }
        auto const resolve{s7_make_function(
            scheme_, "#<sandbox-resolve-script>", load_script_callback, 1, 0, false, nullptr)};
        auto const complete{s7_make_function(
            scheme_, "#<sandbox-complete-script>", complete_load_callback, 0, 0, false, nullptr)};
        auto const load_script{
            s7_apply_function(scheme_, loader_factory_, s7_list(scheme_, 2, resolve, complete))};
        s7_gc_protect(scheme_, load_script);

        auto const load_script_symbol{s7_make_symbol(scheme_, "load-script")};
        s7_define(scheme_, s7_rootlet(scheme_), load_script_symbol, load_script);
        s7_symbol_force_set_initial_value(scheme_, load_script_symbol, load_script);
    }
    ~Impl() {
        {
            std::scoped_lock const lock{instances_mutex_};
            instances_.erase(scheme_);
        }
        s7_free(scheme_);
    }

    [[nodiscard]] auto evaluate(std::string_view const expression) -> EvaluationResult {
        std::string value;
        auto result{evaluate_value(
            expression, &value, [](void* const context, Scheme& scheme, Value const payload) {
                auto& output{*static_cast<std::string*>(context)};
                output = object_to_string(&scheme, payload);
            })};
        if (result.succeeded) {
            result.value = std::move(value);
        }
        return result;
    }

    [[nodiscard]] auto evaluate_value(std::string_view const expression,
                                      void* const context,
                                      ValueConsumer const consume_value) -> EvaluationResult {
        auto const* const source{expression.empty() ? "" : expression.data()};
        auto const source_value{
            s7_make_string_with_length(scheme_, source, static_cast<s7_int>(expression.size()))};
        s7_define(scheme_, s7_rootlet(scheme_), source_symbol_, source_value);

        s7_pointer const result{
            s7_call_with_catch(scheme_, s7_t(scheme_), evaluation_body_, error_handler_)};
        active_loads_.clear();
        active_load_sizes_.clear();
        if (!s7_is_pair(result) || !s7_is_boolean(s7_car(result))) {
            return EvaluationResult{.succeeded = false,
                                    .value = {},
                                    .error = "s7 returned an invalid evaluation result."};
        }

        bool const succeeded{s7_boolean(scheme_, s7_car(result))};
        s7_pointer const payload{s7_cdr(result)};
        if (succeeded) {
            auto const protection{s7_gc_protect(scheme_, payload)};
            consume_value(context, *scheme_, payload);
            s7_gc_unprotect_at(scheme_, protection);
            return EvaluationResult{.succeeded = true, .value = {}, .error = {}};
        }
        if (!s7_is_string(payload)) {
            return EvaluationResult{
                .succeeded = false, .value = {}, .error = "s7 returned an invalid error result."};
        }

        return EvaluationResult{.succeeded = false, .value = {}, .error = s7_string(payload)};
    }
  private:
    static auto load_script_callback(s7_scheme* const scheme, s7_pointer const arguments)
        -> s7_pointer {
        Impl* instance{};
        {
            std::scoped_lock const lock{instances_mutex_};
            auto const found{instances_.find(scheme)};
            if (found != instances_.end()) {
                instance = found->second;
            }
        }
        if (instance == nullptr) {
            return load_failure(scheme, "The script loader is unavailable.");
        }
        return instance->load_script(s7_car(arguments));
    }

    static auto complete_load_callback(s7_scheme* const scheme, s7_pointer) -> s7_pointer {
        Impl* instance{};
        {
            std::scoped_lock const lock{instances_mutex_};
            auto const found{instances_.find(scheme)};
            if (found != instances_.end()) {
                instance = found->second;
            }
        }
        if (instance == nullptr || instance->active_loads_.empty() ||
            instance->active_load_sizes_.empty()) {
            return s7_unspecified(scheme);
        }

        instance->loaded_files_.insert(instance->active_loads_.back());
        instance->loaded_bytes_ += instance->active_load_sizes_.back();
        instance->active_loads_.pop_back();
        instance->active_load_sizes_.pop_back();
        return s7_unspecified(scheme);
    }

    auto load_script(s7_pointer const path_value) -> s7_pointer {
        if (!s7_is_string(path_value)) {
            return load_failure(scheme_, "load-script requires a relative .scm path string.");
        }
        if (!options_.script_library_root_utf8.has_value()) {
            return load_failure(scheme_, "No script library root is configured for this runtime.");
        }

        std::string const requested_path{s7_string(path_value)};
        if (requested_path.empty() || requested_path.front() == '/' ||
            requested_path.front() == '\\' || requested_path.contains(':')) {
            return load_failure(scheme_, "load-script requires a relative .scm path.");
        }
        std::size_t component_start{};
        for (std::size_t index{}; index <= requested_path.size(); ++index) {
            if (index != requested_path.size() && requested_path[index] != '/' &&
                requested_path[index] != '\\') {
                continue;
            }

            auto const component{
                std::string_view{requested_path}.substr(component_start, index - component_start)};
            if (component.empty() || component == "." || component == "..") {
                return load_failure(scheme_, "load-script paths cannot contain '.' or '..'.");
            }
            component_start = index + 1;
        }
        if (!requested_path.ends_with(".scm")) {
            return load_failure(scheme_, "load-script only accepts .scm files.");
        }

        auto const root_source{utf8_to_wide(*options_.script_library_root_utf8)};
        if (!root_source.has_value()) {
            return load_failure(scheme_, "The configured script library root is unavailable.");
        }
        auto const root{canonical_path(*root_source, true)};
        if (!root.has_value()) {
            return load_failure(scheme_, "The configured script library root is unavailable.");
        }

        auto requested_path_wide{utf8_to_wide(requested_path)};
        if (!requested_path_wide.has_value()) {
            return load_failure(scheme_, "The requested script library file is unavailable.");
        }
        std::ranges::replace(*requested_path_wide, L'/', L'\\');
        auto const candidate{canonical_path(root->wide_path + L'\\' + *requested_path_wide, false)};
        if (!candidate.has_value() || !is_within_root(candidate->wide_path, root->wide_path)) {
            return load_failure(scheme_, "The requested script library file is unavailable.");
        }

        auto const key{path_key(candidate->narrow_path)};
        if (loaded_files_.contains(key)) {
            return s7_cons(scheme_, s7_make_integer(scheme_, 1), s7_f(scheme_));
        }
        auto const active{std::ranges::find(active_loads_, key)};
        if (active != active_loads_.end()) {
            std::string cycle{"Recursive load-script cycle: "};
            for (auto iterator{active}; iterator != active_loads_.end(); ++iterator) {
                if (iterator != active) {
                    cycle += " -> ";
                }
                cycle += *iterator;
            }
            cycle += " -> ";
            cycle += key;
            return load_failure(scheme_, cycle);
        }
        if (active_loads_.size() >= options_.max_load_depth) {
            return load_failure(scheme_, "The load-script nesting limit was exceeded.");
        }
        if (loaded_files_.size() + active_loads_.size() >= options_.max_loaded_files) {
            return load_failure(scheme_, "The load-script file count limit was exceeded.");
        }

        auto const file_size{candidate->file_size};
        if (file_size > options_.max_loaded_file_bytes ||
            file_size > std::numeric_limits<std::size_t>::max() - loaded_bytes_ ||
            loaded_bytes_ + file_size > options_.max_total_loaded_bytes) {
            return load_failure(scheme_, "The load-script source size limit was exceeded.");
        }

        active_loads_.push_back(key);
        active_load_sizes_.push_back(file_size);
        return s7_cons(scheme_,
                       s7_make_integer(scheme_, 2),
                       s7_make_string(scheme_, candidate->narrow_path.c_str()));
    }

    inline static std::mutex instances_mutex_{};
    inline static std::unordered_map<s7_scheme*, Impl*> instances_{};

    InterpreterOptions options_;
    s7_scheme* scheme_{};
    s7_pointer source_symbol_{};
    s7_pointer evaluation_body_{};
    s7_pointer error_handler_{};
    s7_pointer safe_format_{};
    s7_pointer loader_factory_{};
    std::unordered_set<std::string> loaded_files_{};
    std::vector<std::string> active_loads_{};
    std::vector<std::size_t> active_load_sizes_{};
    std::size_t loaded_bytes_{};
};

Interpreter::Interpreter()
    : Interpreter{InterpreterOptions{}} {}
Interpreter::Interpreter(InterpreterOptions options)
    : impl_{std::make_unique<Impl>(std::move(options))} {}
Interpreter::~Interpreter() = default;

auto Interpreter::evaluate(std::string_view const expression) -> EvaluationResult {
    return impl_->evaluate(expression);
}
auto Interpreter::evaluate_value_impl(std::string_view const expression,
                                      void* const context,
                                      ValueConsumer const consume_value) -> EvaluationResult {
    return impl_->evaluate_value(expression, context, consume_value);
}
}
