#include <native/s7/interpreter.h>

#include "sandbox_policy.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

namespace ml::s7 {
namespace detail {
auto object_to_string(s7_scheme* const scheme, s7_pointer const value) -> std::string {
    std::unique_ptr<char, decltype(&std::free)> const text{s7_object_to_c_string(scheme, value),
                                                           &std::free};
    return text ? std::string{text.get()} : std::string{};
}
}

Interpreter::Interpreter()
    : Interpreter{InterpreterOptions{}} {}
Interpreter::Interpreter(InterpreterOptions options)
    : scheme_{s7_init(), &s7_free}
    , loader_{options}
    , bindings_{scheme_.get(), loader_, std::move(options.script_library_root_utf8)} {
    if (!scheme_) {
        std::abort();
    }

    // Prepare and immediately protect private helpers before revoking host capabilities.
    source_symbol_ = s7_make_symbol(scheme_.get(), detail::source_variable_name);
    s7_define_variable(
        scheme_.get(), detail::source_variable_name, s7_make_string(scheme_.get(), ""));
    evaluation_body_.emplace(
        scheme_.get(),
        s7_eval_c_string(scheme_.get(),
                         "(lambda () (cons #t (eval-string *sandbox-s7-source* (rootlet))))"));
    error_handler_.emplace(
        scheme_.get(),
        s7_eval_c_string(scheme_.get(), "(lambda (type info) (cons #f (apply format #f info)))"));
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

auto Interpreter::evaluate(std::string_view const expression) -> EvaluationResult {
    std::string value;
    auto result{evaluate_value_impl(
        expression, &value, [](void* const context, Scheme& scheme, Value const payload) {
            auto& output{*static_cast<std::string*>(context)};
            output = detail::object_to_string(&scheme, payload);
        })};
    if (!result) {
        return EvaluationResult{std::unexpect, std::move(result.error())};
    }
    return EvaluationResult{std::in_place, std::move(value)};
}

auto Interpreter::evaluate_value_impl(std::string_view const expression,
                                      void* const context,
                                      ValueConsumer const consume_value) -> ValueEvaluationResult {
    auto const* const source{expression.empty() ? "" : expression.data()};
    auto const source_value{
        s7_make_string_with_length(scheme_.get(), source, static_cast<s7_int>(expression.size()))};
    s7_define(scheme_.get(), s7_rootlet(scheme_.get()), source_symbol_, source_value);

    s7_pointer const result{s7_call_with_catch(
        scheme_.get(), s7_t(scheme_.get()), evaluation_body_->get(), error_handler_->get())};
    loader_.evaluation_ended();
    if (!s7_is_pair(result) || !s7_is_boolean(s7_car(result))) {
        return ValueEvaluationResult{std::unexpect, "s7 returned an invalid evaluation result."};
    }

    bool const succeeded{s7_boolean(scheme_.get(), s7_car(result))};
    s7_pointer const payload{s7_cdr(result)};
    if (succeeded) {
        detail::GcProtection const protection{scheme_.get(), payload};
        consume_value(context, *scheme_, payload);
        return {};
    }
    if (!s7_is_string(payload)) {
        return ValueEvaluationResult{std::unexpect, "s7 returned an invalid error result."};
    }

    return ValueEvaluationResult{std::unexpect, s7_string(payload)};
}
}
