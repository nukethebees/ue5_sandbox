#include "s7_ownership.h"
#include "script_files.h"
#include "script_loader.h"
#include <ioj/s7/interpreter.h>

#include "s7.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace ioj::s7::tests {
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

class TemporaryLibrary final {
  public:
    TemporaryLibrary() {
        auto const unique_suffix{std::chrono::steady_clock::now().time_since_epoch().count()};
        path_ = std::filesystem::temp_directory_path() /
                ("sandbox-s7-library-" + std::to_string(unique_suffix));
        std::filesystem::create_directories(path_);
    }
    ~TemporaryLibrary() {
        std::error_code ignored{};
        std::filesystem::remove_all(path_, ignored);
    }

    TemporaryLibrary(TemporaryLibrary const&) = delete;
    auto operator=(TemporaryLibrary const&) -> TemporaryLibrary& = delete;

    [[nodiscard]] auto path() const -> std::filesystem::path const& { return path_; }

    void write(std::filesystem::path const& relative_path, std::string_view const source) const {
        auto const full_path{path_ / relative_path};
        std::filesystem::create_directories(full_path.parent_path());
        std::ofstream stream{full_path, std::ios::binary};
        stream.write(source.data(), static_cast<std::streamsize>(source.size()));
    }
  private:
    std::filesystem::path path_{};
};

void evaluates_scheme_and_preserves_state(TestContext& test) {
    Interpreter interpreter;

    auto const definition{
        interpreter.evaluate("(begin (define answer-to-everything 6) (* answer-to-everything 7))")};
    test.expect(definition.has_value(), "definition succeeds");
    test.expect(definition == "42", "definition returns 42");

    auto const reuse{interpreter.evaluate("(+ answer-to-everything 1)")};
    test.expect(reuse.has_value(), "state reuse succeeds");
    test.expect(reuse == "7", "state reuse returns 7");
}

void reports_errors_and_remains_usable(TestContext& test) {
    Interpreter interpreter;

    auto const error{interpreter.evaluate("(car 1)")};
    test.expect(!error, "invalid call fails evaluation");
    test.expect(!error && !error.error().empty(), "failed evaluation reports an error");

    auto const recovery{interpreter.evaluate("(+ 20 22)")};
    test.expect(recovery.has_value(), "interpreter recovers after an error");
    test.expect(recovery == "42", "recovery returns 42");
}

void interpreter_instances_have_independent_state(TestContext& test) {
    Interpreter first;
    auto const definition{first.evaluate("(begin (define private-value 17) private-value)")};
    test.expect(definition.has_value(), "first interpreter defines state");

    Interpreter second;
    auto const lookup{second.evaluate("private-value")};
    test.expect(!lookup, "second interpreter cannot see first interpreter state");
    test.expect(!lookup && !lookup.error().empty(), "missing state reports an error");
}

void loader_contexts_are_independent(TestContext& test) {
    TemporaryLibrary first_library;
    TemporaryLibrary second_library;
    first_library.write("value.scm", "(define library-value 17)");
    second_library.write("value.scm", "(define library-value 29)");
    Interpreter second{
        InterpreterOptions{.script_library_root_utf8 = second_library.path().string()}};
    {
        Interpreter first{
            InterpreterOptions{.script_library_root_utf8 = first_library.path().string()}};
        test.expect(first.evaluate(R"((begin (load-script "value.scm") library-value))") == "17",
                    "first loader uses its own context");
        test.expect(second.evaluate(R"((begin (load-script "value.scm") library-value))") == "29",
                    "second loader uses its own context");
    }
    second_library.write("later.scm", "(set! library-value (+ library-value 1))");
    test.expect(second.evaluate(R"((begin (load-script "later.scm") library-value))") == "30",
                "destroying another interpreter preserves loader context");
}

void rejects_unsafe_operations_and_remains_usable(TestContext& test) {
    Interpreter interpreter;
    constexpr std::array<std::string_view, 25> expressions{
        "(exit)",
        "(emergency-exit)",
        "(load \"scenario.scm\")",
        "(require 'scenario)",
        "(open-input-file \"scenario.scm\")",
        "(open-output-file \"scenario.scm\")",
        "(system \"echo unsafe\")",
        "(getenv \"PATH\")",
        "(#_open-input-file \"scenario.scm\")",
        "(#_getenv \"PATH\")",
        "(#_load \"scenario.scm\")",
        "(#_system \"echo unsafe\")",
        "((symbol-initial-value 'open-input-file) \"scenario.scm\")",
        "((#_symbol-initial-value 'getenv) \"PATH\")",
        "((eval '#_getenv) \"PATH\")",
        R"(((eval-string "#_getenv") "PATH"))",
        "(unlet (rootlet) 'load)",
        "(#_rootlet)",
        "(set! (*s7* 'scheme-version) 'r7rs)",
        "(current-input-port)",
        "(c-pointer 0)",
        "(format #t \"host output\")",
        "(delete-file \"scenario.scm\")",
        "(load \"C:/Windows/win.ini\")",
        "(load \"../../../../outside.scm\")",
    };

    for (auto const expression : expressions) {
        auto const result{interpreter.evaluate(expression)};
        test.expect(!result, "unsafe operation fails evaluation");
        test.expect(!result && !result.error().empty(), "unsafe operation reports an error");
    }

    auto const recovery{interpreter.evaluate("(+ 20 22)")};
    test.expect(recovery.has_value(), "interpreter recovers after rejected operations");
    test.expect(recovery == "42", "post-rejection recovery returns 42");

    auto const unconfigured_loader{interpreter.evaluate("(load-script \"helper.scm\")")};
    test.expect(!unconfigured_loader,
                "load-script is unavailable without an approved library root");
}

void retains_useful_general_scheme(TestContext& test) {
    Interpreter interpreter;
    auto const result{
        interpreter.evaluate("(begin"
                             "  (define (square value) (* value value))"
                             "  (define-macro (increment! place) `(set! ,place (+ ,place 1)))"
                             "  (define count 2)"
                             "  (increment! count)"
                             "  (list count"
                             "        (map square '(1 2 3 4))"
                             "        (vector-ref (vector 7 8 9) 1)"
                             "        (format #f \"value-~D\" count)))")};

    test.expect(result.has_value(),
                "functions, macros, collections, and higher-order calls succeed");
    test.expect(result == "(3 (1 4 9 16) 8 \"value-3\")",
                "general Scheme produces the expected value");
}

void loads_libraries_in_the_same_sandbox(TestContext& test) {
    TemporaryLibrary library;
    library.write("inner.scm",
                  "(set! nested-count (+ nested-count 1))\n(define nested-value 40)\n");
    library.write("outer.scm",
                  "(load-script \"inner.scm\")\n"
                  "(define (library-answer value) (+ nested-value value))\n"
                  "(define-macro (library-twice form) `(begin ,form ,form))\n");
    library.write("counted.scm", "(set! load-count (+ load-count 1))\n");
    library.write("unsafe.scm", "(getenv \"PATH\")\n");

    Interpreter interpreter{
        InterpreterOptions{.script_library_root_utf8 = library.path().string()}};
    auto const loaded{
        interpreter.evaluate("(begin"
                             "  (define load-count 0)"
                             "  (define nested-count 0)"
                             "  (define macro-count 0)"
                             "  (load-script \"outer.scm\")"
                             "  (load-script \"inner.scm\")"
                             "  (load-script \"outer.scm\")"
                             "  (load-script \"counted.scm\")"
                             "  (load-script \"counted.scm\")"
                             "  (library-twice (set! macro-count (+ macro-count 1)))"
                             "  (list (library-answer 2) macro-count load-count nested-count))")};
    test.expect(loaded.has_value(),
                "helper and nested helper files load: " + (loaded ? *loaded : loaded.error()));
    test.expect(loaded == "(42 2 1 1)",
                "loaded functions and macros remain visible and files load once");

    auto const unsafe{interpreter.evaluate("(load-script \"unsafe.scm\")")};
    test.expect(!unsafe, "a loaded file has the same sandbox restrictions");
    test.expect(!unsafe && !unsafe.error().empty(),
                "a rejected capability in a loaded file reports an error");
}

void rejects_unsafe_library_paths_and_cycles(TestContext& test) {
    TemporaryLibrary library;
    TemporaryLibrary outside_library;
    library.write("a.scm", "(load-script \"b.scm\")\n");
    library.write("b.scm", "(load-script \"a.scm\")\n");
    library.write("broken.scm", "(car 1)\n");
    library.write("plain.txt", "(define should-not-load #t)\n");
    outside_library.write("secret.scm", "(define escaped-root #t)\n");

    Interpreter interpreter{
        InterpreterOptions{.script_library_root_utf8 = library.path().string()}};
    constexpr std::array<std::string_view, 5> expressions{
        "(load-script \"../outside.scm\")",
        "(load-script \"sub/../outside.scm\")",
        "(load-script \"C:/Windows/win.ini\")",
        "(load-script \"plain.txt\")",
        "(load-script \"missing.scm\")",
    };
    for (auto const expression : expressions) {
        auto const result{interpreter.evaluate(expression)};
        test.expect(!result, "an unsafe or invalid library path is rejected");
        test.expect(!result && !result.error().empty(), "a rejected library path reports an error");
    }

    std::error_code symlink_error;
    std::filesystem::create_directory_symlink(
        outside_library.path(), library.path() / "linked-outside", symlink_error);
    if (!symlink_error) {
        auto const linked_escape{
            interpreter.evaluate("(load-script \"linked-outside/secret.scm\")")};
        test.expect(!linked_escape, "a library symlink cannot escape the approved root");
    }

    auto const cycle{interpreter.evaluate("(load-script \"a.scm\")")};
    test.expect(!cycle, "recursive library loading is rejected");
    test.expect(!cycle && cycle.error().contains("Recursive load-script cycle"),
                "recursive loading reports the cycle");

    auto const broken{interpreter.evaluate("(load-script \"broken.scm\")")};
    test.expect(!broken, "an invalid library reports an evaluation error");
    test.expect(!broken && broken.error().contains("broken.scm"),
                "a library evaluation error retains its filename");

    auto const recovery{interpreter.evaluate("(+ 40 2)")};
    test.expect(recovery == "42", "the interpreter recovers after a load cycle");
}

void enforces_library_resource_limits(TestContext& test) {
    TemporaryLibrary library;
    library.write("large.scm", "(define large-value 123456789)\n");
    library.write("first.scm", "(define first-value 1)\n");
    library.write("second.scm", "(define second-value 2)\n");

    Interpreter interpreter{InterpreterOptions{
        .script_library_root_utf8 = library.path().string(),
        .max_loaded_file_bytes = 8,
    }};
    auto const result{interpreter.evaluate("(load-script \"large.scm\")")};
    test.expect(!result, "an oversized library is rejected");
    test.expect(!result && result.error().contains("size limit"),
                "an oversized library reports its limit");

    Interpreter file_count_interpreter{InterpreterOptions{
        .script_library_root_utf8 = library.path().string(),
        .max_loaded_files = 1,
    }};
    auto const file_count{file_count_interpreter.evaluate(
        R"((begin (load-script "first.scm") (load-script "second.scm")))")};
    test.expect(!file_count, "the library file count is limited");
    test.expect(!file_count && file_count.error().contains("file count limit"),
                "the library file count limit is reported");
}

void caught_child_failure_preserves_parent_and_allows_retry(TestContext& test) {
    TemporaryLibrary library;
    library.write("broken.scm", R"(
        (set! attempts (+ attempts 1))
        (error 'broken "broken child"))");
    library.write("outer.scm", R"(
        (set! outer-count (+ outer-count 1))
        (catch #t (lambda () (load-script "broken.scm")) (lambda args #f))
        (catch #t (lambda () (load-script "broken.scm")) (lambda args #f))
        (set! parent-finished #t))");
    Interpreter interpreter{
        InterpreterOptions{.script_library_root_utf8 = library.path().string()}};
    auto const first{interpreter.evaluate(R"((begin
        (define attempts 0) (define outer-count 0) (define parent-finished #f)
        (load-script "outer.scm")
        (load-script "outer.scm")
        (list attempts outer-count parent-finished)))")};
    test.expect(
        first == "(2 1 #t)",
        "caught child failures can retry immediately and cache only the successful parent: " +
            (first ? *first : first.error()));
    auto const retry{interpreter.evaluate(R"((load-script "broken.scm"))")};
    test.expect(!retry && retry.error().contains("broken.scm"),
                "a failed child is not recorded as successfully loaded");
    auto const count{interpreter.evaluate("attempts")};
    test.expect(count == "3",
                "a later failed-child load executes again: " + (count ? *count : count.error()));
}

void abort_releases_library_reservations(TestContext& test) {
    TemporaryLibrary library;
    std::string const broken{R"((error 'broken "broken"))"};
    library.write("broken.scm", broken);
    library.write("good.scm", "(define recovered 42)");
    Interpreter interpreter{InterpreterOptions{
        .script_library_root_utf8 = library.path().string(),
        .max_total_loaded_bytes = broken.size(),
        .max_loaded_files = 1,
        .max_load_depth = 1,
    }};
    auto const result{interpreter.evaluate(R"((begin
        (catch #t (lambda () (load-script "broken.scm")) (lambda args #f))
        (catch #t (lambda () (load-script "broken.scm")) (lambda args #f))
        (load-script "good.scm") recovered))")};
    test.expect(
        result == "42",
        "abortion releases depth, file count, and byte reservations in the same evaluation");
}

void active_nested_sources_count_toward_byte_limit(TestContext& test) {
    TemporaryLibrary library;
    std::string const outer{R"((load-script "inner.scm"))"};
    std::string const inner{"(define inner-value 42)"};
    library.write("outer.scm", outer);
    library.write("inner.scm", inner);
    Interpreter interpreter{InterpreterOptions{
        .script_library_root_utf8 = library.path().string(),
        .max_total_loaded_bytes = outer.size() + inner.size() - 1,
    }};
    auto const rejected{interpreter.evaluate(R"((load-script "outer.scm"))")};
    test.expect(!rejected && rejected.error().contains("size limit"),
                "parent and child reservations cannot jointly exceed the total byte budget");
    auto const recovery{interpreter.evaluate(R"((begin (load-script "inner.scm") inner-value))")};
    test.expect(recovery == "42",
                "a rejected nested load releases the failed parent's reservation");

    Interpreter exact{InterpreterOptions{
        .script_library_root_utf8 = library.path().string(),
        .max_total_loaded_bytes = outer.size() + inner.size(),
    }};
    auto const accepted{exact.evaluate(R"((begin
        (load-script "outer.scm") (load-script "inner.scm")
        (load-script "outer.scm") inner-value))")};
    test.expect(accepted == "42", "successful nested loads use the exact byte budget once");
}

void nonlocal_exit_aborts_load(TestContext& test) {
    TemporaryLibrary library;
    library.write("escape.scm", "(escape 7)");
    library.write("good.scm", "(define recovered 42)");
    Interpreter interpreter{InterpreterOptions{.script_library_root_utf8 = library.path().string(),
                                               .max_loaded_files = 1,
                                               .max_load_depth = 1}};
    auto const result{interpreter.evaluate(R"((begin
        (define escape #f)
        (call/cc (lambda (k) (set! escape k) (load-script "escape.scm")))
        (load-script "good.scm") recovered))")};
    test.expect(result == "42", "a continuation escape aborts an active load");
}

void loader_transitions_use_identity_and_safe_accounting(TestContext& test) {
    using detail::LoadStatus;
    detail::ScriptLoader loader{
        InterpreterOptions{.max_loaded_file_bytes = std::numeric_limits<std::size_t>::max(),
                           .max_total_loaded_bytes = std::numeric_limits<std::size_t>::max(),
                           .max_loaded_files = 2,
                           .max_load_depth = 2}};
    auto const parent{loader.begin_load("parent", std::numeric_limits<std::size_t>::max() - 1)};
    auto const overflow{loader.begin_load("overflow", 2)};
    test.expect(parent && parent->status == LoadStatus::admitted && !overflow,
                "active reservations reject overflow without adding byte counts");
    if (!parent) {
        return;
    }

    auto const child{loader.begin_load("child", 1)};
    test.expect(child && child->status == LoadStatus::admitted && child->token != parent->token,
                "admitted loads have distinct identities");
    if (!child) {
        return;
    }

    loader.complete_load(parent->token);
    loader.abort_load(child->token);
    auto const cached{loader.begin_load("parent", 0)};
    test.expect(cached && cached->status == LoadStatus::already_loaded,
                "completion identifies the parent even while another token is active");
    auto const retried{loader.begin_load("child", 1)};
    test.expect(retried && retried->status == LoadStatus::admitted &&
                    retried->token != child->token,
                "abort frees count, depth, and bytes and retry gets a new identity");
    if (!retried) {
        return;
    }

    auto const cycle{loader.begin_load("child", 0)};
    test.expect(!cycle && cycle.error().contains("Recursive"),
                "an actually active load still reports a cycle");
    loader.abort_load(retried->token);
    loader.evaluation_ended();
}

void throwing_host_operation_releases_gc_protection(TestContext& test) {
    std::unique_ptr<s7_scheme, decltype(&s7_free)> scheme{s7_init(), &s7_free};
    s7_int first_free_slot{-1};
    for (int iteration{}; iteration < 16; ++iteration) {
        bool caught{false};
        try {
            detail::GcProtection const protected_value{scheme.get(),
                                                       s7_make_integer(scheme.get(), 42)};
            auto const slot{s7_gc_protect(scheme.get(), protected_value.get())};
            s7_gc_unprotect_at(scheme.get(), slot);
            if (first_free_slot < 0) {
                first_free_slot = slot;
            }
            test.expect(slot == first_free_slot,
                        "throwing host operations do not accumulate GC registrations");
            throw std::runtime_error{"host failure"};
        } catch (std::runtime_error const&) {
            caught = true;
        }
        test.expect(caught, "host exceptions propagate");
    }
}

void captured_source_uses_the_validated_file(TestContext& test) {
    TemporaryLibrary library;
    std::string const original{"(define captured-value 42)"};
    library.write("capture.scm", original);
    auto const file{detail::open_script_file(library.path().string(), "capture.scm")};
    test.expect(file.has_value(), "capture opens a validated file");
    if (!file) {
        return;
    }

    std::filesystem::rename(library.path() / "capture.scm", library.path() / "original.scm");
    library.write("capture.scm", "(getenv \"PATH\")");
    auto const source{detail::read_script_source(*file)};
    test.expect(source.has_value() && *source == original,
                "replacing a validated pathname cannot replace the captured source");
}

void source_loading_preserves_reader_errors_and_empty_files(TestContext& test) {
    TemporaryLibrary library;
    library.write("empty.scm", "");
    library.write("reader-error.scm", "(define missing-paren 1");
    Interpreter interpreter{
        InterpreterOptions{.script_library_root_utf8 = library.path().string()}};
    auto const empty{interpreter.evaluate(R"((begin (load-script "empty.scm") 42))")};
    test.expect(empty == "42", "captured empty source loads successfully");
    auto const broken{interpreter.evaluate(R"((load-script "reader-error.scm"))")};
    test.expect(!broken && broken.error().contains("reader-error.scm"),
                "source reader errors preserve filename context");
    library.write("reader-error.scm", "(define repaired 42)");
    auto const repaired{
        interpreter.evaluate(R"((begin (load-script "reader-error.scm") repaired))")};
    test.expect(repaired == "42", "a file can be repaired and retried after a reader error");
}
void internal_hooks_survive_collection_after_bindings_are_revoked(TestContext& test) {
    Interpreter interpreter;
    for (int iteration{}; iteration < 40; ++iteration) {
        auto const result{interpreter.evaluate(
            "(begin (define collected-value (make-list 10000 1)) (length collected-value))")};
        test.expect(result == "10000", "redefinition remains safe across garbage collections");
    }
    test.expect(!interpreter.evaluate("missing-after-collection"),
                "unbound-variable hooks remain alive after collection");
    test.expect(interpreter.evaluate("(+ 20 22)") == "42", "evaluation recovers after an error");
}
}

int main() {
    using namespace ioj::s7::tests;

    TestContext test;
    evaluates_scheme_and_preserves_state(test);
    reports_errors_and_remains_usable(test);
    interpreter_instances_have_independent_state(test);
    loader_contexts_are_independent(test);
    rejects_unsafe_operations_and_remains_usable(test);
    retains_useful_general_scheme(test);
    loads_libraries_in_the_same_sandbox(test);
    rejects_unsafe_library_paths_and_cycles(test);
    enforces_library_resource_limits(test);
    caught_child_failure_preserves_parent_and_allows_retry(test);
    abort_releases_library_reservations(test);
    active_nested_sources_count_toward_byte_limit(test);
    nonlocal_exit_aborts_load(test);
    loader_transitions_use_identity_and_safe_accounting(test);
    throwing_host_operation_releases_gc_protection(test);
    captured_source_uses_the_validated_file(test);
    source_loading_preserves_reader_errors_and_empty_files(test);
    internal_hooks_survive_collection_after_bindings_are_revoked(test);

    return test.failure_count() == 0 ? 0 : 1;
}
