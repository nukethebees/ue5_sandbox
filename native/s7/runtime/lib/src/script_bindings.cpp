#include "script_bindings.h"

#include "s7_ownership.h"
#include "sandbox_policy.h"
#include "script_files.h"

#include <cassert>
#include <limits>
#include <utility>

namespace ioj::s7::detail {
namespace {
auto matches_module(ModuleInput const& input, std::string const& resolved) -> bool {
    std::error_code error;
    return std::filesystem::equivalent(
        input.path, std::filesystem::path{std::u8string{resolved.begin(), resolved.end()}}, error);
}
}

auto prepare_loader_factory(s7_scheme* const scheme) -> s7_pointer {
    // Read and evaluate captured forms inside Scheme so errors and continuation
    // exits reach the wind handler without crossing a resource-owning C++ frame.
    return s7_eval_c_string(scheme, R"(
        (let ((host-evaluate eval)
              (host-open-input-string open-input-string)
              (host-read read)
              (host-eof? eof-object?)
              (host-close-input-port close-input-port)
              (host-sublet sublet)
              (host-let-ref let-ref)
              (host-defined? defined?)
              (sandbox-root (rootlet))
              (host-format format))
          (define (read-module filename source)
            (let ((env (host-sublet sandbox-root))
                  (exports ())
                  (port (host-open-input-string source)))
              (define (declare spec)
                (let* ((renamed (and (list? spec) (= (length spec) 3) (eq? (car spec) 'rename)))
                       (local (if renamed (cadr spec) spec))
                       (public (if renamed (caddr spec) spec)))
                  (unless (and (symbol? local) (symbol? public))
                    (error 'module-error "Invalid export ~S" spec))
                  (when (assq public exports)
                    (error 'module-error "Duplicate export ~S" public))
                  (set! exports (cons (cons public local) exports))))
              (dynamic-wind
                (lambda () #f)
                (lambda ()
                  (catch #t
                    (lambda ()
                      (let loop ((form (host-read port)))
                        (unless (host-eof? form)
                          (if (and (pair? form) (eq? (car form) 'export))
                              (for-each declare (cdr form))
                              (host-evaluate form env))
                          (loop (host-read port))))
                      (for-each
                        (lambda (binding)
                          (unless (host-defined? (cdr binding) env #t)
                            (error 'module-error "Undefined export ~S" (cdr binding))))
                        exports))
                    (lambda (type info)
                      (error type "~A: ~A" filename (apply host-format #f info)))))
                (lambda () (host-close-input-port port)))
              (lambda (name)
                (let ((binding (assq name exports)))
                  (unless binding
                    (error 'module-error "~A does not export ~S" filename name))
                  (host-let-ref env (cdr binding))))))
          (lambda (context resolve complete abort)
            (define (import-resolved resolution)
              (when (string? resolution)
                (error 'module-error "~A" resolution))
              (let ((token (car resolution)) (finished #f))
                (dynamic-wind
                  (lambda ()
                    (when finished
                      (error 'module-error "Cannot reenter a finished module import.")))
                  (lambda ()
                    (let ((module (read-module (cadr resolution) (caddr resolution))))
                      (complete context token)
                      (set! finished #t)
                      module))
                  (lambda ()
                    (unless finished
                      (abort context token)
                      (set! finished #t))))))
            (list (lambda (path) (import-resolved (resolve context path)))
                  (lambda (module name) (module name))
                  (lambda (resolution name)
                    (catch #t
                      (lambda () (cons #t ((import-resolved resolution) name)))
                      (lambda (type info) (cons #f (apply host-format #f info)))))))))");
}

auto load_failure(s7_scheme* const scheme, std::string const& message) -> s7_pointer {
    return s7_make_string(scheme, message.c_str());
}

ScriptBindings::ScriptBindings(s7_scheme* const scheme,
                               ScriptLoader& loader,
                               InterpreterOptions options)
    : scheme_{scheme}
    , loader_{loader}
    , options_{std::move(options)} {}

void ScriptBindings::install(s7_pointer const factory) {
    GcProtection const resolve{
        scheme_,
        s7_make_function(
            scheme_, "#<sandbox-resolve-script>", resolve_callback, 2, 0, false, nullptr)};
    GcProtection const complete{
        scheme_,
        s7_make_function(
            scheme_, "#<sandbox-complete-script>", complete_callback, 2, 0, false, nullptr)};
    GcProtection const abort{
        scheme_,
        s7_make_function(scheme_, "#<sandbox-abort-script>", abort_callback, 2, 0, false, nullptr)};
    GcProtection const owner{scheme_, s7_make_c_pointer(scheme_, this)};
    GcProtection const functions{
        scheme_,
        s7_apply_function(
            scheme_,
            factory,
            s7_list(scheme_, 4, owner.get(), resolve.get(), complete.get(), abort.get()))};
    install_binding(scheme_, "import-module", s7_car(functions.get()));
    install_binding(scheme_, "module-ref", s7_cadr(functions.get()));
    import_export_.emplace(scheme_, s7_caddr(functions.get()));
}
auto ScriptBindings::import_value(std::string_view directory,
                                  std::string_view path,
                                  char const* export_name,
                                  std::string* source) -> Resolution {
    auto value{resolve(directory, path, source)};
    if (!value) {
        return value;
    }
    GcProtection const resolution{scheme_, *value};
    return s7_call(scheme_,
                   import_export_->get(),
                   s7_list(scheme_, 2, resolution.get(), s7_make_symbol(scheme_, export_name)));
}
auto ScriptBindings::context(s7_pointer const arguments) -> ScriptBindings& {
    auto const value{s7_car(arguments)};
    assert(s7_is_c_pointer(value));
    auto* const owner{static_cast<ScriptBindings*>(s7_c_pointer(value))};
    assert(owner != nullptr);
    return *owner;
}
auto ScriptBindings::resolve_callback(s7_scheme* scheme, s7_pointer const arguments) -> s7_pointer {
    auto const path{s7_cadr(arguments)};
    if (!s7_is_string(path)) {
        return load_failure(scheme, "import-module requires a relative .scm path string.");
    }
    auto& bindings{context(arguments)};
    if (!bindings.options_.script_library_root_utf8) {
        return load_failure(scheme, "No script library root is configured for this runtime.");
    }
    auto result{
        bindings.resolve(*bindings.options_.script_library_root_utf8,
                         {s7_string(path), static_cast<std::size_t>(s7_string_length(path))},
                         nullptr)};
    return result ? *result : load_failure(scheme, result.error().message);
}
auto ScriptBindings::complete_callback(s7_scheme* const scheme, s7_pointer const arguments)
    -> s7_pointer {
    auto& bindings{context(arguments)};
    auto const path{bindings.loader_.complete_load({s7_integer(s7_cadr(arguments))})};
    if (auto* input{bindings.options_.module_input}; input && matches_module(*input, path)) {
        input->imported = true;
    }
    return s7_unspecified(scheme);
}
auto ScriptBindings::abort_callback(s7_scheme* const scheme, s7_pointer const arguments)
    -> s7_pointer {
    context(arguments).loader_.abort_load({s7_integer(s7_cadr(arguments))});
    return s7_unspecified(scheme);
}
auto ScriptBindings::resolve(std::string_view directory,
                             std::string_view path,
                             std::string* captured_source) -> Resolution {
    auto file{open_script_file(directory, path)};
    if (!file) {
        return Resolution{
            std::unexpect, AstErrorCode::FileOpenFailed, "$", std::move(file.error())};
    }
    auto const* input{options_.module_input};
    auto const replacement{input && input->draft && matches_module(*input, file->narrow_path)
                               ? input->draft
                               : std::nullopt};
    auto const size{replacement ? replacement->size() : file->file_size};
    if (size > static_cast<std::size_t>(std::numeric_limits<s7_int>::max())) {
        return Resolution{std::unexpect,
                          AstErrorCode::LimitExceeded,
                          "$",
                          "The import-module source size limit was exceeded."};
    }

    auto admission{loader_.begin_load(path_key(file->narrow_path), size)};
    if (!admission) {
        return Resolution{
            std::unexpect, AstErrorCode::EvaluationFailed, "$", std::move(admission.error())};
    }

    auto source{replacement ? ScriptSourceResult{std::in_place, *replacement}
                            : read_script_source(*file)};
    if (!source) {
        loader_.abort_load(*admission);
        return Resolution{
            std::unexpect, AstErrorCode::FileReadFailed, "$", std::move(source.error())};
    }
    GcProtection const filename{scheme_,
                                s7_make_string(scheme_, path_key(file->narrow_path).c_str())};
    std::string_view text{*source};
    if (text.starts_with("\xef\xbb\xbf")) {
        text.remove_prefix(3);
    }
    GcProtection const contents{
        scheme_,
        s7_make_string_with_length(scheme_, text.data(), static_cast<s7_int>(text.size()))};
    GcProtection const token{scheme_, s7_make_integer(scheme_, admission->value)};
    if (captured_source) {
        *captured_source = std::move(*source);
    }
    return s7_list(scheme_, 3, token.get(), filename.get(), contents.get());
}
}
