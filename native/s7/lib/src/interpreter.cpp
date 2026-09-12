#include <native/s7/interpreter.h>

#include "s7.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace ml::s7 {
namespace {
constexpr char source_variable_name[]{"*sandbox-s7-source*"};
constexpr char permission_error_name[]{"permission-error"};

auto disabled_operation(s7_scheme* const scheme, s7_pointer) -> s7_pointer {
    return s7_error(
        scheme,
        s7_make_symbol(scheme, permission_error_name),
        s7_list(scheme,
                1,
                s7_make_string(scheme, "This operation is disabled by the embedded s7 runtime.")));
}

void disable_unsafe_operations(s7_scheme& scheme) {
    constexpr char const* operations[]{
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

    for (auto const* const operation : operations) {
        s7_define_function(&scheme,
                           operation,
                           disabled_operation,
                           0,
                           0,
                           true,
                           "Disabled by the embedded s7 runtime.");
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
}

class Interpreter::Impl final {
  public:
    Impl() {
        scheme_ = s7_init();
        if (scheme_ == nullptr) {
            std::abort();
        }

        disable_unsafe_operations(*scheme_);

        source_symbol_ = s7_make_symbol(scheme_, source_variable_name);
        s7_define_variable(scheme_, source_variable_name, s7_make_string(scheme_, ""));

        evaluation_body_ = s7_eval_c_string(
            scheme_, "(lambda () (cons #t (eval-string *sandbox-s7-source* (rootlet))))");
        error_handler_ =
            s7_eval_c_string(scheme_, "(lambda (type info) (cons #f (apply format #f info)))");
        s7_gc_protect(scheme_, evaluation_body_);
        s7_gc_protect(scheme_, error_handler_);
    }
    ~Impl() { s7_free(scheme_); }

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
        s7_symbol_set_value(scheme_, source_symbol_, source_value);

        s7_pointer const result{
            s7_call_with_catch(scheme_, s7_t(scheme_), evaluation_body_, error_handler_)};
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
    s7_scheme* scheme_{};
    s7_pointer source_symbol_{};
    s7_pointer evaluation_body_{};
    s7_pointer error_handler_{};
};

Interpreter::Interpreter()
    : impl_{std::make_unique<Impl>()} {}
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
