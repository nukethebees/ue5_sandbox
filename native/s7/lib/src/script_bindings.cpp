#include <ioj/s7/detail/script_bindings.h>

#include "sandbox_policy.h"
#include "script_files_win32.h"
#include <ioj/s7/detail/s7_ownership.h>

#include <cassert>
#include <limits>
#include <utility>

namespace ioj::s7::detail {
auto prepare_loader_factory(s7_scheme* const scheme) -> s7_pointer {
    // Read and evaluate captured forms inside Scheme so errors and continuation
    // exits reach the wind handler without crossing a resource-owning C++ frame.
    return s7_eval_c_string(scheme, R"(
        (let ((host-evaluate eval)
              (host-open-input-string open-input-string)
              (host-read read)
              (host-eof? eof-object?)
              (host-close-input-port close-input-port)
              (sandbox-root (rootlet))
              (host-format format)
              (no-value (if #f #f)))
          (lambda (resolve complete abort)
            (lambda (path)
              (let* ((resolution (resolve path))
                     (status (car resolution)))
                (cond ((= status 0) (error 'load-script-error (cdr resolution)))
                      ((= status 1) no-value)
                      (else
                        (let ((token (cadr resolution))
                              (filename (caddr resolution))
                              (port (host-open-input-string (cadddr resolution)))
                              (finished #f))
                          (dynamic-wind
                            (lambda ()
                              (if finished
                                  (error 'load-script-error "Cannot reenter a finished script load.")))
                            (lambda ()
                              (catch #t
                                (lambda ()
                                  (let loop ((form (host-read port)))
                                    (unless (host-eof? form)
                                      (host-evaluate form sandbox-root)
                                      (loop (host-read port))))
                                  (complete token)
                                  (set! finished #t)
                                  no-value)
                                (lambda (type info)
                                  (error type "~A: ~A" filename (apply host-format #f info)))))
                            (lambda ()
                              (host-close-input-port port)
                              (unless finished
                                (abort token)
                                (set! finished #t))))))))))))");
}

auto load_failure(s7_scheme* const scheme, std::string const& message) -> s7_pointer {
    return s7_cons(scheme, s7_make_integer(scheme, 0), s7_make_string(scheme, message.c_str()));
}

ScriptBindings::ScriptBindings(s7_scheme* const scheme,
                               ScriptLoader& loader,
                               std::optional<std::string> root)
    : scheme_{scheme}
    , loader_{loader}
    , root_{std::move(root)} {}
ScriptBindings::~ScriptBindings() {
    std::scoped_lock const lock{instances_mutex_};
    instances_.erase(scheme_);
}
void ScriptBindings::install(s7_pointer const factory) {
    {
        std::scoped_lock const lock{instances_mutex_};
        assert(!instances_.contains(scheme_));
        instances_.emplace(scheme_, this);
    }

    GcProtection const resolve{
        scheme_,
        s7_make_function(
            scheme_, "#<sandbox-resolve-script>", resolve_callback, 1, 0, false, nullptr)};
    GcProtection const complete{
        scheme_,
        s7_make_function(
            scheme_, "#<sandbox-complete-script>", complete_callback, 1, 0, false, nullptr)};
    GcProtection const abort{
        scheme_,
        s7_make_function(scheme_, "#<sandbox-abort-script>", abort_callback, 1, 0, false, nullptr)};
    GcProtection const load_script{
        scheme_,
        s7_apply_function(
            scheme_, factory, s7_list(scheme_, 3, resolve.get(), complete.get(), abort.get()))};
    install_binding(scheme_, "load-script", load_script.get());
}
auto ScriptBindings::context(s7_scheme* const scheme) -> ScriptBindings& {
    std::scoped_lock const lock{instances_mutex_};
    auto const found{instances_.find(scheme)};
    assert(found != instances_.end());
    return *found->second;
}
auto ScriptBindings::resolve_callback(s7_scheme* const scheme, s7_pointer const arguments)
    -> s7_pointer {
    return context(scheme).resolve(s7_car(arguments));
}
auto ScriptBindings::complete_callback(s7_scheme* const scheme, s7_pointer const arguments)
    -> s7_pointer {
    context(scheme).loader_.complete_load({s7_integer(s7_car(arguments))});
    return s7_unspecified(scheme);
}
auto ScriptBindings::abort_callback(s7_scheme* const scheme, s7_pointer const arguments)
    -> s7_pointer {
    context(scheme).loader_.abort_load({s7_integer(s7_car(arguments))});
    return s7_unspecified(scheme);
}
auto ScriptBindings::resolve(s7_pointer const path) -> s7_pointer {
    // Return errors as data: never non-locally exit across file/string owners.
    if (!s7_is_string(path)) {
        return load_failure(scheme_, "load-script requires a relative .scm path string.");
    }
    if (!root_) {
        return load_failure(scheme_, "No script library root is configured for this runtime.");
    }

    auto const file{open_script_file(
        *root_, {s7_string(path), static_cast<std::size_t>(s7_string_length(path))})};
    if (!file) {
        return load_failure(scheme_, file.error());
    }
    if (file->file_size > static_cast<std::size_t>(std::numeric_limits<s7_int>::max())) {
        return load_failure(scheme_, "The load-script source size limit was exceeded.");
    }

    auto const admission{loader_.begin_load(path_key(file->narrow_path), file->file_size)};
    if (!admission) {
        return load_failure(scheme_, admission.error());
    }
    if (admission->status == LoadStatus::already_loaded) {
        return s7_cons(scheme_, s7_make_integer(scheme_, 1), s7_f(scheme_));
    }

    auto const source{read_script_source(*file)};
    if (!source) {
        loader_.abort_load(admission->token);
        return load_failure(scheme_, source.error());
    }
    GcProtection const filename{scheme_, s7_make_string(scheme_, file->narrow_path.c_str())};
    GcProtection const contents{
        scheme_,
        s7_make_string_with_length(scheme_, source->c_str(), static_cast<s7_int>(source->size()))};
    GcProtection const token{scheme_, s7_make_integer(scheme_, admission->token.value)};
    return s7_list(
        scheme_, 4, s7_make_integer(scheme_, 2), token.get(), filename.get(), contents.get());
}
}
