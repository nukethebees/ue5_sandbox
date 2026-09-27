from tidy_test_support import TidyTest, VIEWS, main


class LoopViewAccessorCallTests(TidyTest):
    check = "ioj-loop-view-accessor-call"

    def test_accessors(self) -> None:
        self.assert_cases(VIEWS, (
            ("member", "Items items; for (;;) { auto v = items.positions(); }", 1),
            ("free_arbitrary_name", "for (;;) { consume(resolve()); }", 1),
            ("braced", "for (;;) { auto const v{resolve()}; }", 1),
            ("reference", "for (;;) { auto const& v = borrowed(); }", 1),
            ("range", "int a[2]{}; for (int i : a) { resolve(); }", 1),
            ("while", "while (true) { resolve(); }", 1),
            ("do", "do { resolve(); } while (true);", 1),
            ("condition", "while ((resolve(), true)) {}", 1),
            ("increment", "for (;;resolve()) {}", 1),
            ("hoisted", "auto v = resolve(); for (;;) { consume(v); }", 0),
            ("initializer", "for (auto v = resolve();;) {}", 0),
            ("ordinary", "Items items; for (;;) { items.ordinary(); items.misleading_view(); }", 0),
            ("lambda", "for (;;) { auto f = [] { resolve(); }; }", 0),
            ("immediate_lambda", "for (;;) { [] { resolve(); }(); }", 0),
            ("lambda_return", "for (;;) { auto v = [] { return resolve(); }(); }", 1),
            ("lambda_loop", "auto f = [] { for (;;) { resolve(); } };", 1),
            ("lambda_capture", "for (;;) { auto f = [v = resolve()] {}; }", 1),
            ("local_method", "for (;;) { struct Local { void f() { resolve(); } }; }", 0),
            ("unevaluated", "for (;;) { (void)sizeof(resolve()); (void)noexcept(resolve()); }", 0),
            ("suppressed", "for (;;) {\n// NOLINTNEXTLINE(ioj-loop-view-accessor-call)\nresolve();\n}", 0),
        ))

    def test_diagnostic_ownership(self) -> None:
        checks = "ioj-loop-view-construction,ioj-loop-view-accessor-call"
        self.assert_cases(VIEWS, (
            ("explicit", "for (;;) { consume(View{}); }", 0),
            ("call", "for (;;) { auto v{resolve()}; }", 1),
            ("copy", "View v; for (;;) { consume(v); }", 0),
        ), checks)


if __name__ == "__main__":
    main(LoopViewAccessorCallTests)
