#include "s7_ownership.h"
#include "script_files.h"
#include "script_loader.h"
#include <ioj/s7/interpreter.h>

#include "s7.h"
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace ioj::s7::tests {
void expect_error(Interpreter& interpreter,
                  std::string_view source,
                  std::string_view expected = {}) {
    SCOPED_TRACE(source);
    auto const result{interpreter.evaluate(source)};
    ASSERT_FALSE(result);
    EXPECT_FALSE(result.error().empty());
    EXPECT_TRUE(result.error().contains(expected)) << result.error();
}

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

    auto make_interpreter(InterpreterOptions options = {}) const -> Interpreter {
        options.script_library_root_utf8 = path_.string();
        return Interpreter{std::move(options)};
    }

    void write(std::filesystem::path const& relative_path, std::string_view const source) const {
        auto const full_path{path_ / relative_path};
        std::filesystem::create_directories(full_path.parent_path());
        std::ofstream stream{full_path, std::ios::binary};
        stream.write(source.data(), static_cast<std::streamsize>(source.size()));
    }
  private:
    std::filesystem::path path_{};
};

TEST(SchemeInterpreter, EvaluatesSchemeAndPreservesState) {
    Interpreter interpreter;

    auto const definition{
        interpreter.evaluate("(begin (define answer-to-everything 6) (* answer-to-everything 7))")};
    EXPECT_EQ(definition, "42");

    EXPECT_EQ(interpreter.evaluate("(+ answer-to-everything 1)"), "7");
}

TEST(SchemeInterpreter, ReportsErrorsAndRemainsUsable) {
    Interpreter interpreter;

    expect_error(interpreter, "(car 1)");
    EXPECT_EQ(interpreter.evaluate("(+ 20 22)"), "42");
}

TEST(SchemeInterpreter, InterpreterInstancesHaveIndependentState) {
    Interpreter first;
    auto const definition{first.evaluate("(begin (define private-value 17) private-value)")};
    EXPECT_TRUE(definition.has_value());

    Interpreter second;
    expect_error(second, "private-value");
}

TEST(SchemeInterpreter, LoaderContextsAreIndependent) {
    TemporaryLibrary first_library;
    TemporaryLibrary second_library;
    first_library.write("value.scm", "(export value) (define value 17)");
    second_library.write("value.scm", "(export value) (define value 29)");
    auto second{second_library.make_interpreter()};
    constexpr auto lookup{R"((module-ref (import-module "value.scm") 'value))"};
    {
        auto first{first_library.make_interpreter()};
        EXPECT_EQ(first.evaluate(lookup), "17");
        EXPECT_EQ(second.evaluate(lookup), "29");
    }
    second_library.write("value.scm", "(export value) (define value 30)");
    EXPECT_EQ(second.evaluate(lookup), "30");
}

TEST(SchemeInterpreter, RejectsUnsafeOperationsAndRemainsUsable) {
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
        expect_error(interpreter, expression);
    }

    EXPECT_EQ(interpreter.evaluate("(+ 20 22)"), "42");
    EXPECT_FALSE(interpreter.evaluate("(import-module \"helper.scm\")"));
}

TEST(SchemeInterpreter, RetainsUsefulGeneralScheme) {
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

    EXPECT_EQ(result, "(3 (1 4 9 16) 8 \"value-3\")");
}

TEST(SchemeInterpreter, ImportsPrivateStateAndResolvesDeclaredExports) {
    TemporaryLibrary library;
    library.write("counter.scm", R"(
        (export (rename next advance) current)
        (define count 0)
        (define (next) (set! count (+ count 1)) count)
        (define (current) count))");
    library.write("duplicate.scm", "(export public (rename private public))");
    library.write("missing.scm", "(export absent)");
    library.write("unsafe.scm", R"((getenv "PATH"))");

    auto interpreter{library.make_interpreter()};
    auto const result{interpreter.evaluate(R"((begin
        (define first (import-module "counter.scm"))
        (define second (import-module "counter.scm"))
        ((module-ref first 'advance))
        ((module-ref first 'advance))
        (list ((module-ref first 'current)) ((module-ref second 'current)))))")};
    EXPECT_EQ(result, "(2 0)");
    expect_error(interpreter, "(module-ref first 'count)", "counter.scm");
    expect_error(interpreter, "(module-ref first 'next)", "counter.scm");
    expect_error(interpreter, "count");
    expect_error(interpreter, R"((import-module "duplicate.scm"))", "duplicate.scm");
    expect_error(interpreter, R"((import-module "missing.scm"))", "missing.scm");
    expect_error(interpreter, R"((import-module "unsafe.scm"))", "unsafe.scm");
}

TEST(SchemeInterpreter, RejectsUnsafeLibraryPathsAndCycles) {
    TemporaryLibrary library;
    TemporaryLibrary outside_library;
    library.write("a.scm", "(import-module \"b.scm\")\n");
    library.write("b.scm", "(import-module \"a.scm\")\n");
    library.write("broken.scm", "(car 1)\n");
    library.write("plain.txt", "(define should-not-load #t)\n");
    outside_library.write("secret.scm", "(define escaped-root #t)\n");

    auto interpreter{library.make_interpreter()};
    constexpr std::array<std::string_view, 5> expressions{
        "(import-module \"../outside.scm\")",
        "(import-module \"sub/../outside.scm\")",
        "(import-module \"C:/Windows/win.ini\")",
        "(import-module \"plain.txt\")",
        "(import-module \"missing.scm\")",
    };
    for (auto const expression : expressions) {
        expect_error(interpreter, expression);
    }

    std::error_code symlink_error;
    std::filesystem::create_directory_symlink(
        outside_library.path(), library.path() / "linked-outside", symlink_error);
    if (!symlink_error) {
        EXPECT_FALSE(interpreter.evaluate("(import-module \"linked-outside/secret.scm\")"));
    }

    expect_error(interpreter, "(import-module \"a.scm\")", "Recursive import-module cycle");
    expect_error(interpreter, "(import-module \"broken.scm\")", "broken.scm");
    EXPECT_EQ(interpreter.evaluate("(+ 40 2)"), "42");
}

TEST(SchemeInterpreter, EnforcesLibraryResourceLimits) {
    TemporaryLibrary library;
    library.write("large.scm", "(define large-value 123456789)\n");
    library.write("first.scm", "(define first-value 1)\n");
    library.write("second.scm", "(define second-value 2)\n");

    auto interpreter{library.make_interpreter({.max_loaded_file_bytes = 8})};
    expect_error(interpreter, "(import-module \"large.scm\")", "size limit");

    auto file_count_interpreter{library.make_interpreter({.max_loaded_files = 1})};
    expect_error(file_count_interpreter,
                 R"((begin (import-module "first.scm") (import-module "second.scm")))",
                 "file count limit");
}

TEST(SchemeInterpreter, CaughtChildFailurePreservesParentAndAllowsRetry) {
    TemporaryLibrary library;
    library.write("broken.scm", R"(
        (set! attempts (+ attempts 1))
        (error 'broken "broken child"))");
    library.write("outer.scm", R"(
        (set! outer-count (+ outer-count 1))
        (catch #t (lambda () (import-module "broken.scm")) (lambda args #f))
        (catch #t (lambda () (import-module "broken.scm")) (lambda args #f))
        (set! parent-finished #t))");
    auto interpreter{library.make_interpreter()};
    auto const first{interpreter.evaluate(R"((begin
        (define attempts 0) (define outer-count 0) (define parent-finished #f)
        (import-module "outer.scm")
        (import-module "outer.scm")
        (list attempts outer-count parent-finished)))")};
    EXPECT_EQ(first, "(4 2 #t)");
    expect_error(interpreter, R"((import-module "broken.scm"))", "broken.scm");
    EXPECT_EQ(interpreter.evaluate("attempts"), "5");
}

TEST(SchemeInterpreter, AbortReleasesLibraryReservations) {
    TemporaryLibrary library;
    std::string const broken{R"((error 'broken "long failed load for the budget"))"};
    library.write("broken.scm", broken);
    library.write("good.scm", "(export recovered) (define recovered 42)");
    auto interpreter{library.make_interpreter({
        .max_total_loaded_bytes = broken.size(),
        .max_loaded_files = 1,
        .max_load_depth = 1,
    })};
    auto const result{interpreter.evaluate(R"((begin
        (catch #t (lambda () (import-module "broken.scm")) (lambda args #f))
        (catch #t (lambda () (import-module "broken.scm")) (lambda args #f))
        (module-ref (import-module "good.scm") 'recovered)))")};
    EXPECT_EQ(result, "42");
}

TEST(SchemeInterpreter, ActiveNestedSourcesCountTowardByteLimit) {
    TemporaryLibrary library;
    std::string const outer{
        R"((export value) (define value (module-ref (import-module "inner.scm") 'value)))"};
    std::string const inner{"(export value) (define value 42)"};
    library.write("outer.scm", outer);
    library.write("inner.scm", inner);
    auto interpreter{library.make_interpreter({
        .max_total_loaded_bytes = outer.size() + inner.size() - 1,
    })};
    expect_error(interpreter, R"((import-module "outer.scm"))", "size limit");
    auto const recovery{interpreter.evaluate(R"((module-ref (import-module "inner.scm") 'value))")};
    EXPECT_EQ(recovery, "42");

    auto exact{library.make_interpreter({
        .max_total_loaded_bytes = outer.size() + inner.size(),
    })};
    auto const accepted{exact.evaluate(R"((module-ref (import-module "outer.scm") 'value))")};
    EXPECT_EQ(accepted, "42");
    auto const reimported{exact.import_ast(library.path().string(), "outer.scm", "value")};
    ASSERT_TRUE(reimported.has_value());
}

TEST(SchemeInterpreter, NonlocalExitAbortsLoad) {
    TemporaryLibrary library;
    library.write("escape.scm", "(escape 7)");
    library.write("good.scm", "(export recovered) (define recovered 42)");
    auto interpreter{library.make_interpreter({.max_loaded_files = 1, .max_load_depth = 1})};
    auto const result{interpreter.evaluate(R"((begin
        (define escape #f)
        (call/cc (lambda (k) (set! escape k) (import-module "escape.scm")))
        (module-ref (import-module "good.scm") 'recovered)))")};
    EXPECT_EQ(result, "42");
}

TEST(SchemeInterpreter, LoaderTransitionsUseIdentityAndSafeAccounting) {
    detail::ScriptLoader loader{
        InterpreterOptions{.max_loaded_file_bytes = std::numeric_limits<std::size_t>::max(),
                           .max_total_loaded_bytes = std::numeric_limits<std::size_t>::max(),
                           .max_loaded_files = 2,
                           .max_load_depth = 2}};
    auto const parent{loader.begin_load("parent", std::numeric_limits<std::size_t>::max() - 1)};
    auto const overflow{loader.begin_load("overflow", 2)};
    ASSERT_TRUE(parent);
    EXPECT_FALSE(overflow);

    auto const child{loader.begin_load("child", 1)};
    ASSERT_TRUE(child);
    EXPECT_NE(*child, *parent);

    loader.complete_load(*parent);
    loader.abort_load(*child);
    auto const repeated{loader.begin_load("parent", 0)};
    ASSERT_TRUE(repeated);
    EXPECT_NE(*repeated, *parent);
    loader.abort_load(*repeated);
    auto const retried{loader.begin_load("child", 1)};
    ASSERT_TRUE(retried);
    EXPECT_NE(*retried, *child);

    auto const cycle{loader.begin_load("child", 0)};
    EXPECT_TRUE(!cycle && cycle.error().contains("Recursive"));
    loader.abort_load(*retried);
    loader.evaluation_ended();
}

TEST(SchemeInterpreter, ThrowingHostOperationReleasesGcProtection) {
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
            EXPECT_EQ(slot, first_free_slot);
            throw std::runtime_error{"host failure"};
        } catch (std::runtime_error const&) {
            caught = true;
        }
        EXPECT_TRUE(caught);
    }
}

TEST(SchemeInterpreter, CapturedSourceUsesTheValidatedFile) {
    TemporaryLibrary library;
    std::string const original{"(define captured-value 42)"};
    library.write("capture.scm", original);
    auto const file{detail::open_script_file(library.path().string(), "capture.scm")};
    ASSERT_TRUE(file);

    std::filesystem::rename(library.path() / "capture.scm", library.path() / "original.scm");
    library.write("capture.scm", "(getenv \"PATH\")");
    auto const source{detail::read_script_source(*file)};
    EXPECT_EQ(source, original);
}

TEST(SchemeInterpreter, SourceLoadingPreservesReaderErrorsAndEmptyFiles) {
    TemporaryLibrary library;
    library.write("empty.scm", "");
    library.write("reader-error.scm", "(define missing-paren 1");
    auto interpreter{library.make_interpreter()};
    auto const empty{interpreter.evaluate(R"((begin (import-module "empty.scm") 42))")};
    EXPECT_EQ(empty, "42");
    expect_error(interpreter, R"((import-module "reader-error.scm"))", "reader-error.scm");
    library.write("reader-error.scm", "(export repaired) (define repaired 42)");
    auto const repaired{
        interpreter.evaluate(R"((module-ref (import-module "reader-error.scm") 'repaired))")};
    EXPECT_EQ(repaired, "42");
}
TEST(SchemeInterpreter, InternalHooksSurviveCollectionAfterBindingsAreRevoked) {
    Interpreter interpreter;
    for (int iteration{}; iteration < 40; ++iteration) {
        auto const result{interpreter.evaluate(
            "(begin (define collected-value (make-list 10000 1)) (length collected-value))")};
        EXPECT_EQ(result, "10000");
    }
    EXPECT_FALSE(interpreter.evaluate("missing-after-collection"));
    EXPECT_EQ(interpreter.evaluate("(+ 20 22)"), "42");
}
}
