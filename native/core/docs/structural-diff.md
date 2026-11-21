# Compare structured values

Use `ioj::DiffReport` from `<ioj/structural_diff.h>` for field paths and mismatched values. Floats compare exactly. Pointers compare by identity.

In a GoogleTest target linked to `native-test-support`, include `<ioj/testing/structural_diff.h>`:

```cpp
EXPECT_PRED_FORMAT2(ioj::testing::structurally_equal, expected, actual);
```
