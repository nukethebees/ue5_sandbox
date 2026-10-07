#pragma once

#include <ioj/s7/ast.h>
#include <ioj/s7/detail/s7_ownership.h>
#include <ioj/s7/detail/script_bindings.h>
#include <ioj/s7/detail/script_loader.h>
#include <ioj/s7/value.h>

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace ioj::s7 {
using EvaluationResult = std::expected<std::string, std::string>;
using ValueEvaluationResult = std::expected<void, std::string>;

struct InterpreterOptions {
    std::optional<std::string> script_library_root_utf8{};
    std::size_t max_loaded_file_bytes{1024 * 1024};
    std::size_t max_total_loaded_bytes{8 * 1024 * 1024};
    std::size_t max_loaded_files{64};
    std::size_t max_load_depth{32};
};

class Interpreter {
  public:
    Interpreter();
    explicit Interpreter(InterpreterOptions options);
    ~Interpreter();

    Interpreter(Interpreter const&) = delete;
    auto operator=(Interpreter const&) -> Interpreter& = delete;

    [[nodiscard]] auto evaluate(std::string_view expression) -> EvaluationResult;

    [[nodiscard]] auto evaluate_ast(std::string_view expression, ioj::s7::AstLimits limits = {})
        -> ioj::s7::AstResult;

    // The consumer runs synchronously while the value is GC-protected and must not retain handles.
    template <typename Consumer>
    [[nodiscard]] auto evaluate_value(std::string_view const expression, Consumer&& consumer)
        -> ValueEvaluationResult {
        using ConsumerType = std::remove_reference_t<Consumer>;
        auto* const context{const_cast<void*>(static_cast<void const*>(std::addressof(consumer)))};
        return evaluate_value_impl(
            expression, context, [](void* const raw_context, Scheme& scheme, Value const value) {
                (*static_cast<ConsumerType*>(raw_context))(scheme, value);
            });
    }
  private:
    using ValueConsumer = void (*)(void* context, Scheme& scheme, Value value);

    [[nodiscard]] auto evaluate_value_impl(std::string_view expression,
                                           void* context,
                                           ValueConsumer consume_value) -> ValueEvaluationResult;

    std::unique_ptr<Scheme, void (*)(Scheme*)> scheme_;
    detail::ScriptLoader loader_;
    detail::ScriptBindings bindings_;
    Value source_symbol_{};
    std::optional<detail::GcProtection> evaluation_body_;
    std::optional<detail::GcProtection> error_handler_;
};
}
