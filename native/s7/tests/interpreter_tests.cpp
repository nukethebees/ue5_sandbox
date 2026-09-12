#include <native/s7/interpreter.h>
#include <native/s7/value.h>

#include <array>
#include <iostream>
#include <string_view>

namespace ml::s7::tests {
class TestContext final {
  public:
    void expect(bool const condition, std::string_view const message) {
        if (!condition) {
            std::cerr << "FAILED: " << message << '\n';
            ++failure_count_;
        }
    }

    [[nodiscard]] auto failure_count() const -> int { return failure_count_; }
  private:
    int failure_count_{};
};

void evaluates_scheme_and_preserves_state(TestContext& test) {
    Interpreter interpreter;

    auto const definition{
        interpreter.evaluate("(begin (define answer-to-everything 6) (* answer-to-everything 7))")};
    test.expect(definition.succeeded, "definition succeeds");
    test.expect(definition.value == "42", "definition returns 42");
    test.expect(definition.error.empty(), "successful definition has no error");

    auto const reuse{interpreter.evaluate("(+ answer-to-everything 1)")};
    test.expect(reuse.succeeded, "state reuse succeeds");
    test.expect(reuse.value == "7", "state reuse returns 7");
}

void reports_errors_and_remains_usable(TestContext& test) {
    Interpreter interpreter;

    auto const error{interpreter.evaluate("(car 1)")};
    test.expect(!error.succeeded, "invalid call fails evaluation");
    test.expect(error.value.empty(), "failed evaluation has no value");
    test.expect(!error.error.empty(), "failed evaluation reports an error");

    auto const recovery{interpreter.evaluate("(+ 20 22)")};
    test.expect(recovery.succeeded, "interpreter recovers after an error");
    test.expect(recovery.value == "42", "recovery returns 42");
}

void interpreter_instances_have_independent_state(TestContext& test) {
    Interpreter first;
    auto const definition{first.evaluate("(begin (define private-value 17) private-value)")};
    test.expect(definition.succeeded, "first interpreter defines state");

    Interpreter second;
    auto const lookup{second.evaluate("private-value")};
    test.expect(!lookup.succeeded, "second interpreter cannot see first interpreter state");
    test.expect(!lookup.error.empty(), "missing state reports an error");
}

void rejects_unsafe_operations_and_remains_usable(TestContext& test) {
    Interpreter interpreter;
    constexpr std::array<std::string_view, 6> expressions{
        "(exit)",
        "(emergency-exit)",
        "(load \"scenario.scm\")",
        "(open-input-file \"scenario.scm\")",
        "(open-output-file \"scenario.scm\")",
        "(system \"echo unsafe\")",
    };

    for (auto const expression : expressions) {
        auto const result{interpreter.evaluate(expression)};
        test.expect(!result.succeeded, "unsafe operation fails evaluation");
        test.expect(result.error.contains("disabled by the embedded s7 runtime"),
                    "unsafe operation reports the runtime restriction");
    }

    auto const recovery{interpreter.evaluate("(+ 20 22)")};
    test.expect(recovery.succeeded, "interpreter recovers after rejected operations");
    test.expect(recovery.value == "42", "post-rejection recovery returns 42");
}

void exposes_values_during_a_synchronous_callback(TestContext& test) {
    Interpreter interpreter;
    bool consumed{};

    auto const result{interpreter.evaluate_value(
        "(list 'level \"title\" 42.5)", [&](Scheme& scheme, Value const value) {
            consumed = true;
            test.expect(is_list(scheme, value), "root value is a list");
            test.expect(list_length(scheme, value) == 3, "list length is exposed");

            auto const tag{list_value(scheme, value, 0)};
            test.expect(is_symbol(tag), "first value is a symbol");
            test.expect(symbol_name(tag) == "level", "symbol name is exposed");

            auto const title{list_value(scheme, value, 1)};
            test.expect(is_string(title), "second value is a string");
            test.expect(string_value(title) == "title", "string value is exposed");

            auto const number{list_value(scheme, value, 2)};
            test.expect(is_real(number), "third value is real");
            test.expect(number_to_real(scheme, number) == 42.5, "real value is exposed");
        })};

    test.expect(result.succeeded, "value evaluation succeeds");
    test.expect(result.error.empty(), "value evaluation has no error");
    test.expect(consumed, "value callback is invoked");
}
}

int main() {
    using namespace ml::s7::tests;

    TestContext test;
    evaluates_scheme_and_preserves_state(test);
    reports_errors_and_remains_usable(test);
    interpreter_instances_have_independent_state(test);
    rejects_unsafe_operations_and_remains_usable(test);
    exposes_values_during_a_synchronous_callback(test);

    return test.failure_count() == 0 ? 0 : 1;
}
