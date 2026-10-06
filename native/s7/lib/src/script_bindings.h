#pragma once

#include "script_loader.h"

#include "s7.h"

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace ml::s7::detail {
[[nodiscard]] auto prepare_loader_factory(s7_scheme* scheme) -> s7_pointer;

// The runtime and loader outlive this registration. Serialize all access to one
// interpreter, including destruction; the registry mutex only protects lookup
// across distinct interpreters, never evaluation or the returned context.
class ScriptBindings final {
  public:
    ScriptBindings(s7_scheme* scheme, ScriptLoader& loader, std::optional<std::string> root);
    ~ScriptBindings();
    ScriptBindings(ScriptBindings const&) = delete;
    auto operator=(ScriptBindings const&) -> ScriptBindings& = delete;

    void install(s7_pointer factory);
  private:
    static auto context(s7_scheme* scheme) -> ScriptBindings&;
    static auto resolve_callback(s7_scheme* scheme, s7_pointer arguments) -> s7_pointer;
    static auto complete_callback(s7_scheme* scheme, s7_pointer arguments) -> s7_pointer;
    static auto abort_callback(s7_scheme* scheme, s7_pointer arguments) -> s7_pointer;
    auto resolve(s7_pointer path) -> s7_pointer;

    inline static std::mutex instances_mutex_;
    inline static std::unordered_map<s7_scheme*, ScriptBindings*> instances_;
    s7_scheme* scheme_;
    ScriptLoader& loader_;
    std::optional<std::string> root_;
};
}
