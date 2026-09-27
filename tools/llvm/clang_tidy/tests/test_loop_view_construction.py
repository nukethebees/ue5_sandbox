from tidy_test_support import TidyTest, VIEWS, main


class LoopViewConstructionTests(TidyTest):
    check = "ioj-loop-view-construction"

    def test_constructions(self) -> None:
        self.assert_cases(VIEWS, (
            ("for", "for (;;) { View v; }", 1),
            ("range", "int a[2]{}; for (int i : a) { View v{}; }", 1),
            ("while", "while (true) { View v(nullptr, nullptr, nullptr, 0); }", 1),
            ("do", "do { auto v = View{}; } while (true);", 1),
            ("temporary", "for (;;) { consume(View{}); }", 1),
            ("span", "for (;;) { std::span<int> s(nullptr, 0); }", 1),
            ("compact_base", "for (;;) { ioj::sim::Compact v; }", 1),
            ("aggregate", "for (;;) { ioj::sim::LineTracesConstView v{}; }", 1),
            ("hoisted", "View v; for (;;) { consume(v); }", 0),
            ("ordinary", "for (;;) { OrdinaryView v; ioj::sim::PlayerReadView snapshot{}; }", 0),
            ("copies", "View v; for (;;) { View copy(v); auto& ref = v; consume(v); }", 0),
            ("accessor", "for (;;) { auto v = resolve(); }", 0),
            ("braced_accessor", "for (;;) { auto const v{resolve()}; }", 0),
            ("initializer", "for (View v;;) {}", 0),
            ("nested_initializer", "for (;;) { for (View v;;) {} }", 1),
            ("lambda", "for (;;) { auto f = [] { View v; }; }", 0),
            ("immediate_lambda", "for (;;) { [] { View v; }(); }", 0),
            ("lambda_loop", "auto f = [] { for (;;) { View v; } };", 1),
            ("lambda_capture", "for (;;) { auto f = [v = View{}] {}; }", 1),
            ("local_method", "for (;;) { struct Local { void f() { View v; } }; }", 0),
            ("unevaluated", "for (;;) { (void)sizeof(View{}); (void)noexcept(View{}); }", 0),
            ("nested_loop", "for (;;) { while (true) { View v; } }", 1),
            ("suppressed", "for (;;) {\n// NOLINTNEXTLINE(ioj-loop-view-construction)\nView v;\n}", 0),
        ))


if __name__ == "__main__":
    main(LoopViewConstructionTests)
