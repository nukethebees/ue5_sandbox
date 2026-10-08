#pragma once

#include <ioj/s7/detail/script_loader.h>
#include <ioj/s7/value.h>

#include <optional>
#include <string>

namespace ioj::s7::detail {
[[nodiscard]] auto prepare_loader_factory(Scheme* scheme) -> Value;

// The runtime and loader outlive the private Scheme closure that captures this instance.
// Serialize evaluation and destruction of each interpreter.
class ScriptBindings {
  public:
    ScriptBindings(Scheme* scheme, ScriptLoader& loader, std::optional<std::string> root);
    ScriptBindings(ScriptBindings const&) = delete;
    auto operator=(ScriptBindings const&) -> ScriptBindings& = delete;

    void install(Value factory);
  private:
    static auto context(Value arguments) -> ScriptBindings&;
    static auto resolve_callback(Scheme* scheme, Value arguments) -> Value;
    static auto complete_callback(Scheme* scheme, Value arguments) -> Value;
    static auto abort_callback(Scheme* scheme, Value arguments) -> Value;
    auto resolve(Value path) -> Value;

    Scheme* scheme_;
    ScriptLoader& loader_;
    std::optional<std::string> root_;
};
}
