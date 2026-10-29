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
    first_library.write("value.scm", "(define library-value 17)");
    second_library.write("value.scm", "(define library-value 29)");
    auto second{second_library.make_interpreter()};
    {
        auto first{first_library.make_interpreter()};
        EXPECT_EQ(first.evaluate(R"((begin (load-script "value.scm") library-value))"), "17");
        EXPECT_EQ(second.evaluate(R"((begin (load-script "value.scm") library-value))"), "29");
    }
    second_library.write("later.scm", "(set! library-value (+ library-value 1))");
    EXPECT_EQ(second.evaluate(R"((begin (load-script "later.scm") library-value))"), "30");
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
    EXPECT_FALSE(interpreter.evaluate("(load-script \"helper.scm\")"));
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

TEST(SchemeInterpreter, LoadsLibrariesInTheSameSandbox) {
    TemporaryLibrary library;
    library.write("inner.scm",
                  "(set! nested-count (+ nested-count 1))\n(define nested-value 40)\n");
    library.write("outer.scm",
                  "(load-script \"inner.scm\")\n"
                  "(define (library-answer value) (+ nested-value value))\n"
                  "(define-macro (library-twice form) `(begin ,form ,form))\n");
    library.write("counted.scm", "(set! load-count (+ load-count 1))\n");
    library.write("unsafe.scm", "(getenv \"PATH\")\n");
    library.write("uppercase.SCM", "(set! load-count (+ load-count 1))\nload-count");

    auto interpreter{library.make_interpreter()};
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
    EXPECT_EQ(loaded, "(42 2 1 1)");

    EXPECT_EQ(interpreter.evaluate(R"((load-script "uppercase.SCM"))"), "2");
    EXPECT_EQ(interpreter.evaluate(R"((load-script "UPPERCASE.scm"))"), "2");

    expect_error(interpreter, "(load-script \"unsafe.scm\")");
}

TEST(SchemeInterpreter, RejectsUnsafeLibraryPathsAndCycles) {
    TemporaryLibrary library;
    TemporaryLibrary outside_library;
    library.write("a.scm", "(load-script \"b.scm\")\n");
    library.write("b.scm", "(load-script \"a.scm\")\n");
    library.write("broken.scm", "(car 1)\n");
    library.write("plain.txt", "(define should-not-load #t)\n");
    outside_library.write("secret.scm", "(define escaped-root #t)\n");

    auto interpreter{library.make_interpreter()};
    constexpr std::array<std::string_view, 5> expressions{
        "(load-script \"../outside.scm\")",
        "(load-script \"sub/../outside.scm\")",
        "(load-script \"C:/Windows/win.ini\")",
        "(load-script \"plain.txt\")",
        "(load-script \"missing.scm\")",
    };
    for (auto const expression : expressions) {
        expect_error(interpreter, expression);
    }

    std::error_code symlink_error;
    std::filesystem::create_directory_symlink(
        outside_library.path(), library.path() / "linked-outside", symlink_error);
    if (!symlink_error) {
        EXPECT_FALSE(interpreter.evaluate("(load-script \"linked-outside/secret.scm\")"));
    }

    expect_error(interpreter, "(load-script \"a.scm\")", "Recursive load-script cycle");
    expect_error(interpreter, "(load-script \"broken.scm\")", "broken.scm");
    EXPECT_EQ(interpreter.evaluate("(+ 40 2)"), "42");
}

TEST(SchemeInterpreter, EnforcesLibraryResourceLimits) {
    TemporaryLibrary library;
    library.write("large.scm", "(define large-value 123456789)\n");
    library.write("first.scm", "(define first-value 1)\n");
    library.write("second.scm", "(define second-value 2)\n");

    auto interpreter{library.make_interpreter({.max_loaded_file_bytes = 8})};
    expect_error(interpreter, "(load-script \"large.scm\")", "size limit");

    auto file_count_interpreter{library.make_interpreter({.max_loaded_files = 1})};
    expect_error(file_count_interpreter,
                 R"((begin (load-script "first.scm") (load-script "second.scm")))",
                 "file count limit");
}

TEST(SchemeInterpreter, CaughtChildFailurePreservesParentAndAllowsRetry) {
    TemporaryLibrary library;
    library.write("broken.scm", R"(
        (set! attempts (+ attempts 1))
        (error 'broken "broken child"))");
    library.write("outer.scm", R"(
        (set! outer-count (+ outer-count 1))
        (catch #t (lambda () (load-script "broken.scm")) (lambda args #f))
        (catch #t (lambda () (load-script "broken.scm")) (lambda args #f))
        (set! parent-finished #t))");
    auto interpreter{library.make_interpreter()};
    auto const first{interpreter.evaluate(R"((begin
        (define attempts 0) (define outer-count 0) (define parent-finished #f)
        (load-script "outer.scm")
        (load-script "outer.scm")
        (list attempts outer-count parent-finished)))")};
    EXPECT_EQ(first, "(2 1 #t)");
    expect_error(interpreter, R"((load-script "broken.scm"))", "broken.scm");
    EXPECT_EQ(interpreter.evaluate("attempts"), "3");
}

TEST(SchemeInterpreter, AbortReleasesLibraryReservations) {
    TemporaryLibrary library;
    std::string const broken{R"((error 'broken "broken"))"};
    library.write("broken.scm", broken);
    library.write("good.scm", "(define recovered 42)");
    auto interpreter{library.make_interpreter({
        .max_total_loaded_bytes = broken.size(),
        .max_loaded_files = 1,
        .max_load_depth = 1,
    })};
    auto const result{interpreter.evaluate(R"((begin
        (catch #t (lambda () (load-script "broken.scm")) (lambda args #f))
        (catch #t (lambda () (load-script "broken.scm")) (lambda args #f))
        (load-script "good.scm") recovered))")};
    EXPECT_EQ(result, "42");
}

TEST(SchemeInterpreter, ActiveNestedSourcesCountTowardByteLimit) {
    TemporaryLibrary library;
    std::string const outer{R"((load-script "inner.scm"))"};
    std::string const inner{"(define inner-value 42)"};
    library.write("outer.scm", outer);
    library.write("inner.scm", inner);
    auto interpreter{library.make_interpreter({
        .max_total_loaded_bytes = outer.size() + inner.size() - 1,
    })};
    expect_error(interpreter, R"((load-script "outer.scm"))", "size limit");
    auto const recovery{interpreter.evaluate(R"((begin (load-script "inner.scm") inner-value))")};
    EXPECT_EQ(recovery, "42");

    auto exact{library.make_interpreter({
        .max_total_loaded_bytes = outer.size() + inner.size(),
    })};
    auto const accepted{exact.evaluate(R"((begin
        (load-script "outer.scm") (load-script "inner.scm")
        (load-script "outer.scm") inner-value))")};
    EXPECT_EQ(accepted, "42");
}

TEST(SchemeInterpreter, NonlocalExitAbortsLoad) {
    TemporaryLibrary library;
    library.write("escape.scm", "(escape 7)");
    library.write("good.scm", "(define recovered 42)");
    auto interpreter{library.make_interpreter({.max_loaded_files = 1, .max_load_depth = 1})};
    auto const result{interpreter.evaluate(R"((begin
        (define escape #f)
        (call/cc (lambda (k) (set! escape k) (load-script "escape.scm")))
        (load-script "good.scm") recovered))")};
    EXPECT_EQ(result, "42");
}

TEST(SchemeInterpreter, LoaderTransitionsUseIdentityAndSafeAccounting) {
    using detail::LoadStatus;
    detail::ScriptLoader loader{
        InterpreterOptions{.max_loaded_file_bytes = std::numeric_limits<std::size_t>::max(),
                           .max_total_loaded_bytes = std::numeric_limits<std::size_t>::max(),
                           .max_loaded_files = 2,
                           .max_load_depth = 2}};
    auto const parent{loader.begin_load("parent", std::numeric_limits<std::size_t>::max() - 1)};
    auto const overflow{loader.begin_load("overflow", 2)};
    ASSERT_TRUE(parent);
    EXPECT_EQ(parent->status, LoadStatus::admitted);
    EXPECT_FALSE(overflow);

    auto const child{loader.begin_load("child", 1)};
    ASSERT_TRUE(child);
    EXPECT_EQ(child->status, LoadStatus::admitted);
    EXPECT_NE(child->token, parent->token);

    loader.complete_load(parent->token);
    loader.abort_load(child->token);
    auto const cached{loader.begin_load("parent", 0)};
    EXPECT_TRUE(cached && cached->status == LoadStatus::already_loaded);
    auto const retried{loader.begin_load("child", 1)};
    ASSERT_TRUE(retried);
    EXPECT_EQ(retried->status, LoadStatus::admitted);
    EXPECT_NE(retried->token, child->token);

    auto const cycle{loader.begin_load("child", 0)};
    EXPECT_TRUE(!cycle && cycle.error().contains("Recursive"));
    loader.abort_load(retried->token);
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
    auto const empty{interpreter.evaluate(R"((begin (load-script "empty.scm") 42))")};
    EXPECT_EQ(empty, "42");
    expect_error(interpreter, R"((load-script "reader-error.scm"))", "reader-error.scm");
    library.write("reader-error.scm", "(define repaired 42)");
    auto const repaired{
        interpreter.evaluate(R"((begin (load-script "reader-error.scm") repaired))")};
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
