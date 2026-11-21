#include <ioj/s7/interpreter.h>

#include "ast_materialization.h"
#include "s7_ownership.h"
#include "sandbox_policy.h"
#include "script_bindings.h"
#include "script_loader.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

namespace ioj::s7 {
namespace detail {
auto make_scheme() -> s7_scheme* {
    auto* const scheme{s7_init()};
    if (!scheme) {
        std::abort();
    }
    return scheme;
}
auto prepare_evaluation_body(s7_scheme* const scheme) -> s7_pointer {
    s7_define_variable(scheme, source_variable_name, s7_make_string(scheme, ""));
    return s7_eval_c_string(scheme,
                            "(lambda () (cons #t (eval-string *sandbox-s7-source* (rootlet))))");
}

auto object_to_string(s7_scheme* const scheme, s7_pointer const value) -> std::string {
    std::unique_ptr<char, decltype(&std::free)> const text{s7_object_to_c_string(scheme, value),
                                                           &std::free};
    return text ? std::string{text.get()} : std::string{};
}
}

struct Interpreter::Impl {
    using ValueResult = std::expected<s7_pointer, std::string>;
    explicit Impl(InterpreterOptions options);
    auto evaluate_value(std::string_view expression) -> ValueResult;
    auto unpack(s7_pointer result) -> ValueResult;
    auto ast(ValueResult value, AstLimits limits) -> AstResult;

    std::unique_ptr<s7_scheme, void (*)(s7_scheme*)> scheme_;
    detail::ScriptLoader loader_;
    detail::ScriptBindings bindings_;
    s7_pointer source_symbol_{};
    detail::GcProtection evaluation_body_;
    detail::GcProtection error_handler_;
    detail::GcProtection runtime_hooks_;
};

Interpreter::Interpreter()
    : Interpreter{InterpreterOptions{}} {}
Interpreter::Interpreter(InterpreterOptions options)
    : impl_{std::make_unique<Impl>(std::move(options))} {}

Interpreter::Impl::Impl(InterpreterOptions options)
    : scheme_{detail::make_scheme(), &s7_free}
    , loader_{options}
    , bindings_{scheme_.get(), loader_, std::move(options)}
    , source_symbol_{s7_make_symbol(scheme_.get(), detail::source_variable_name)}
    , evaluation_body_{scheme_.get(), detail::prepare_evaluation_body(scheme_.get())}
    , error_handler_{scheme_.get(),
                     s7_eval_c_string(scheme_.get(),
                                      "(lambda (type info) (cons #f (apply format #f info)))")}
    // Preserve hooks referenced internally by s7 after their public bindings are revoked.
    , runtime_hooks_{
          scheme_.get(),
          s7_eval_c_string(
              scheme_.get(),
              "(list *unbound-variable-hook* *missing-close-paren-hook* *error-hook* "
              "*load-hook* *autoload-hook* *read-error-hook* *rootlet-redefinition-hook*)")} {
    // Protect private helpers before revoking host capabilities.
    detail::GcProtection const safe_format{scheme_.get(),
                                           detail::prepare_safe_format(scheme_.get())};
    detail::GcProtection const loader_factory{scheme_.get(),
                                              detail::prepare_loader_factory(scheme_.get())};
    detail::GcProtection const disabled{scheme_.get(),
                                        detail::prepare_disabled_operation(scheme_.get())};

    detail::restrict_global_environment(*scheme_, disabled.get());

    detail::install_binding(scheme_.get(), "format", safe_format.get());
    bindings_.install(loader_factory.get());
}
Interpreter::~Interpreter() = default;

auto Interpreter::evaluate_ast(std::string_view const expression, ioj::s7::AstLimits const limits)
    -> ioj::s7::AstResult {
    return impl_->ast(impl_->evaluate_value(expression), limits);
}

auto Interpreter::import_ast(std::string_view directory,
                             std::string_view path,
                             char const* export_name,
                             std::string* source) -> AstResult {
    impl_->loader_.reset_import_budget();
    if (source) {
        source->clear();
    }
    auto result{impl_->bindings_.import_value(directory, path, export_name, source)};
    if (!result) {
        return AstResult{std::unexpect, {std::move(result.error())}};
    }
    return impl_->ast(impl_->unpack(*result), {});
}

auto Interpreter::Impl::ast(ValueResult value, AstLimits limits) -> AstResult {
    if (!value) {
        return AstResult{
            std::unexpect,
            {AstDiagnostic{AstErrorCode::EvaluationFailed, "$", std::move(value.error())}}};
    }
    detail::GcProtection const protection{scheme_.get(), *value};
    return detail::materialize_ast(*scheme_, *value, limits);
}

auto Interpreter::evaluate(std::string_view const expression) -> EvaluationResult {
    return impl_->evaluate_value(expression).transform([&](s7_pointer value) {
        detail::GcProtection const protection{impl_->scheme_.get(), value};
        return detail::object_to_string(impl_->scheme_.get(), value);
    });
}

auto Interpreter::Impl::evaluate_value(std::string_view expression) -> ValueResult {
    auto const* const source{expression.empty() ? "" : expression.data()};
    auto const source_value{
        s7_make_string_with_length(scheme_.get(), source, static_cast<s7_int>(expression.size()))};
    s7_define(scheme_.get(), s7_rootlet(scheme_.get()), source_symbol_, source_value);

    s7_pointer const result{s7_call_with_catch(
        scheme_.get(), s7_t(scheme_.get()), evaluation_body_.get(), error_handler_.get())};
    return unpack(result);
}

auto Interpreter::Impl::unpack(s7_pointer result) -> ValueResult {
    loader_.evaluation_ended();
    if (!s7_is_pair(result) || !s7_is_boolean(s7_car(result))) {
        return ValueResult{std::unexpect, "s7 returned an invalid evaluation result."};
    }

    bool const succeeded{s7_boolean(scheme_.get(), s7_car(result))};
    s7_pointer const payload{s7_cdr(result)};
    if (succeeded) {
        return payload;
    }
    if (!s7_is_string(payload)) {
        return ValueResult{std::unexpect, "s7 returned an invalid error result."};
    }

    return ValueResult{std::unexpect, s7_string(payload)};
}
}
