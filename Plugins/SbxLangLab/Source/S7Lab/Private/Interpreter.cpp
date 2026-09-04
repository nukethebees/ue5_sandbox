#include "S7Lab/Interpreter.h"

#include "s7.h"

#include <Containers/StringConv.h>
#include <Math/Vector.h>
#include <Misc/AssertionMacros.h>

#include <cstdlib>

namespace S7Lab {
namespace {
constexpr ANSICHAR source_variable_name[]{"*sbx-lang-lab-source*"};
constexpr ANSICHAR permission_error_name[]{"permission-error"};

auto disabled_operation(s7_scheme* const scheme, s7_pointer) -> s7_pointer {
    return s7_error(
        scheme,
        s7_make_symbol(scheme, permission_error_name),
        s7_list(
            scheme, 1, s7_make_string(scheme, "This operation is disabled by the S7Lab sandbox.")));
}

void disable_unsafe_operations(s7_scheme& scheme) {
    constexpr ANSICHAR const* operations[]{
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
        s7_define_function(
            &scheme, operation, disabled_operation, 0, 0, true, "Disabled by the S7Lab sandbox.");
    }
}

auto to_fstring(ANSICHAR const* const value) -> FString {
    auto const converted{FUTF8ToTCHAR{value}};
    return FString{converted.Length(), converted.Get()};
}

auto vector_length_squared(s7_scheme* const scheme, s7_pointer const arguments) -> s7_pointer {
    s7_pointer const x{s7_car(arguments)};
    s7_pointer const y{s7_cadr(arguments)};
    s7_pointer const z{s7_caddr(arguments)};

    if (!s7_is_number(x)) {
        return s7_wrong_type_arg_error(scheme, "sbx-vector-length-squared", 1, x, "a number");
    }
    if (!s7_is_number(y)) {
        return s7_wrong_type_arg_error(scheme, "sbx-vector-length-squared", 2, y, "a number");
    }
    if (!s7_is_number(z)) {
        return s7_wrong_type_arg_error(scheme, "sbx-vector-length-squared", 3, z, "a number");
    }

    FVector const vector{
        s7_number_to_real(scheme, x), s7_number_to_real(scheme, y), s7_number_to_real(scheme, z)};
    return s7_make_real(scheme, vector.SizeSquared());
}

auto uppercase(s7_scheme* const scheme, s7_pointer const arguments) -> s7_pointer {
    s7_pointer const value{s7_car(arguments)};
    if (!s7_is_string(value)) {
        return s7_wrong_type_arg_error(scheme, "sbx-uppercase", 1, value, "a string");
    }

    FString upper{to_fstring(s7_string(value))};
    upper.ToUpperInline();
    auto const converted{FTCHARToUTF8{*upper}};
    return s7_make_string_with_length(scheme, converted.Get(), converted.Length());
}

auto object_to_fstring(s7_scheme* const scheme, s7_pointer const value) -> FString {
    ANSICHAR* const text{s7_object_to_c_string(scheme, value)};
    if (text == nullptr) {
        return {};
    }

    FString result{to_fstring(text)};
    std::free(text);
    return result;
}
}

class FInterpreter::FImpl final {
  public:
    FImpl() {
        scheme_ = s7_init();
        checkf(scheme_ != nullptr, TEXT("s7 failed to initialise."));

        disable_unsafe_operations(*scheme_);

        source_symbol_ = s7_make_symbol(scheme_, source_variable_name);
        s7_define_variable(scheme_, source_variable_name, s7_make_string(scheme_, ""));

        evaluation_body_ = s7_eval_c_string(
            scheme_, "(lambda () (cons #t (eval-string *sbx-lang-lab-source* (rootlet))))");
        error_handler_ =
            s7_eval_c_string(scheme_, "(lambda (type info) (cons #f (apply format #f info)))");
        s7_gc_protect(scheme_, evaluation_body_);
        s7_gc_protect(scheme_, error_handler_);

        s7_define_function(scheme_,
                           "sbx-vector-length-squared",
                           vector_length_squared,
                           3,
                           0,
                           false,
                           "Return the squared length of an Unreal vector.");
        s7_define_function(scheme_,
                           "sbx-uppercase",
                           uppercase,
                           1,
                           0,
                           false,
                           "Return an Unreal FString converted to uppercase.");
    }

    ~FImpl() { s7_free(scheme_); }

    auto evaluate(FStringView const expression) -> FEvaluationResult {
        FString value;
        auto result{evaluate_value(expression, [&value, this](s7_pointer const payload) {
            value = object_to_fstring(scheme_, payload);
        })};
        result.value = MoveTemp(value);
        return result;
    }

    auto evaluate_value(FStringView const expression,
                        TFunctionRef<void(s7_pointer)> const consume_value) -> FEvaluationResult {
        auto const converted{FTCHARToUTF8{expression.GetData(), expression.Len()}};
        auto const source{s7_make_string_with_length(scheme_, converted.Get(), converted.Length())};
        s7_symbol_set_value(scheme_, source_symbol_, source);

        s7_pointer const result{
            s7_call_with_catch(scheme_, s7_t(scheme_), evaluation_body_, error_handler_)};
        if (!s7_is_pair(result) || !s7_is_boolean(s7_car(result))) {
            return {.error = TEXT("s7 returned an invalid evaluation result.")};
        }

        bool const succeeded{s7_boolean(scheme_, s7_car(result))};
        s7_pointer const payload{s7_cdr(result)};
        if (succeeded) {
            auto const protection{s7_gc_protect(scheme_, payload)};
            consume_value(payload);
            s7_gc_unprotect_at(scheme_, protection);
            return {.succeeded = true};
        }
        if (!s7_is_string(payload)) {
            return {.error = TEXT("s7 returned an invalid error result.")};
        }

        return {.error = to_fstring(s7_string(payload))};
    }

    auto native_handle() noexcept -> s7_scheme* { return scheme_; }
  private:
    s7_scheme* scheme_{};
    s7_pointer source_symbol_{};
    s7_pointer evaluation_body_{};
    s7_pointer error_handler_{};
};

FInterpreter::FInterpreter()
    : impl_{MakeUnique<FImpl>()} {}

FInterpreter::~FInterpreter() = default;

auto FInterpreter::evaluate(FStringView const expression) -> FEvaluationResult {
    return impl_->evaluate(expression);
}

auto
    FInterpreter::evaluate_value(FStringView const expression,
                                 TFunctionRef<void(s7_scheme&, native::FValue)> const consume_value)
        -> FEvaluationResult {
    return impl_->evaluate_value(expression, [this, &consume_value](native::FValue const value) {
        consume_value(*native_handle(), value);
    });
}

auto FInterpreter::native_handle() noexcept -> s7_scheme* {
    return impl_->native_handle();
}
}
