# Structural comparisons

Include `<ioj/structural_diff.h>` to compare values with diagnostic paths:

```cpp
ioj::DiffResult differences;
ioj::DiffContext context{differences};
context.diff("fighters", expected, actual);
EXPECT_TRUE(differences.empty()) << differences.message();
```

For example:

```text
fighters.laser.damage
  Expected: 5
  Actual:   10
```

Lispb records opt in with `:generate-visitor true`. Every member participates, including
S7-excluded fields. The diff visits the expected member once and selects the actual member
with `get_field<F>()`. It supports nested records, vectors (including `vector<bool>`), arrays,
optionals and `ConstrainedValue` wrappers. Sequence length differences appear at `.size`;
elements in the common prefix use `[index]`. Optional presence is reported as present/absent.

Other equality-comparable types are opaque leaves: their `operator==` determines equality,
and the path stops at that object. Enable visitation or provide an ADL `diff_members` function
when member diagnostics or specialized semantics are required:

```cpp
void diff_members(Position const& expected, Position const& actual, ioj::DiffContext& context) {
    context.diff("x", expected.x(), actual.x());
    context.diff("y", expected.y(), actual.y());
}
```

ADL customization takes precedence over generated visitation. Unsupported noncomparable types
produce a compile-time diagnostic. This utility does not discover arbitrary aggregates or
Unreal-reflected properties. It does not follow pointers: pointer leaves compare identities.
There is no tolerance policy; exact floating-point equality makes NaNs unequal and signed
zeros equal. Specialized scene equivalence and ownership assertions should remain explicit.

Path components borrow names only for the duration of `diff()`. The context restores its path
after exceptions, and retained differences own their strings. Successful comparisons format
no diagnostic strings; the reusable path stack can allocate as nesting depth grows. Values
use `std::format` when supported, enum underlying values otherwise, and an explicit
`<unformattable value>` fallback for other unequal leaves. `DiffResult::limit` defaults to 64
stored differences; traversal continues and counts additional mismatches in `omitted`.

Boolean equality remains separate. Use Lispb `:comparison equality` on suitable value records
to generate defaulted `operator==`; no diagnostic machinery is involved in that operation.

Native GoogleTest targets can link `native-test-support` and include
`<ioj/testing/structural_diff.h>` for a single assertion with the same diagnostics:

```cpp
EXPECT_PRED_FORMAT2(ioj::testing::structurally_equal, expected, actual);
```

Use `ASSERT_PRED_FORMAT2` when subsequent steps depend on equality. Both forms report at the
call site and include the operand expressions. Prefer an existing original/default object or
a concise expected aggregate; keep tolerance, ownership and lifecycle assertions explicit.
