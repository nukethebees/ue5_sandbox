#pragma once

#include <native/s7/value.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace ml::s7 {
struct EvaluationResult {
    bool succeeded{};
    std::string value;
    std::string error;
};

struct InterpreterOptions {
    std::optional<std::string> script_library_root_utf8{};
    std::size_t max_loaded_file_bytes{1024 * 1024};
    std::size_t max_total_loaded_bytes{8 * 1024 * 1024};
    std::size_t max_loaded_files{64};
    std::size_t max_load_depth{32};
};

class Interpreter final {
  public:
    Interpreter();
    explicit Interpreter(InterpreterOptions options);
    ~Interpreter();

    Interpreter(Interpreter const&) = delete;
    auto operator=(Interpreter const&) -> Interpreter& = delete;

    [[nodiscard]] auto evaluate(std::string_view expression) -> EvaluationResult;

    // The consumer runs synchronously while the value is GC-protected and must not retain handles.
    template <typename Consumer>
    [[nodiscard]] auto evaluate_value(std::string_view const expression, Consumer&& consumer)
        -> EvaluationResult {
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
                                           ValueConsumer consume_value) -> EvaluationResult;

    class Impl;
    std::unique_ptr<Impl> impl_;
};
}
