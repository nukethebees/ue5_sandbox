# Native enum formatting

Opt in through the existing helper list:

```lisp
(enum State std::uint8_t
  :native-api true
  :helpers (to-string to-serialized-string to-display-string formatter)
  (value Ready :serialized-name "ready" :display-name "Ready to start"))
```

This emits a `std::formatter<State>` specialization inheriting `ioj::EnumFormatter<State>`.
The shared formatter calls existing string helpers; no string tables or value switches are
duplicated. Enums without `formatter` retain their existing output and dependencies.

| Selector | Representation | Required helper |
|---|---|---|
| `{}` or `{:n}` | C++ enumerator name | `to-string` |
| `{:s}` | Serialized spelling | `to-serialized-string` |
| `{:l}` | Display label | `to-display-string` |

`formatter` requires `to-string`. Other representations are optional; selecting an unavailable
one is a format error, not a fallback. This supports narrow-character formatting, including
`std::format_to`.

String conversions require known, unique, dense numeric values starting at zero. Declaration
order may differ from numeric order. Lispb checks this contract before generation. Lookup
arguments must be declared enum values; debug builds assert on out-of-range values, and release
builds perform no validity checks. A declared count sentinel without a serialized name retains
its `<invalid EnumName>` string.

Each requested representation emits one `ioj::enum_strings<Enum, Category>` specialization:
a null-terminated character blob and offsets, including a final sentinel. Lispb calculates the
offsets and selects `uint8_t`, `uint16_t`, or `uint32_t` storage. The shared
`ioj::enum_string<ioj::Name/Display/Serialized>(value)` performs two offset loads and returns a
string view. Native helpers reuse this function; Unreal exports retain thin adapters over the
same lookup with `TCHAR` data. Enums used only as numeric codes do not require string tables.

An optional selector comes **before** the ordinary string format specification:

```cpp
std::format("parameter '{}' is declared as {:s}, not {:s}", name, actual, expected);
std::format("{:s>12}", State::Ready);  // Right-aligned serialized spelling.
std::format("{:l.{}}", State::Ready, 5);  // Display label with dynamic precision.
```

Width, alignment, fill, precision, and dynamic argument indexing use the standard string
formatter. Leading `n`, `s`, and `l` select representations; use an explicit selector before
those fill characters, for example `{:ns>12}` for names padded with `s`. Numeric presentation
specifiers are not supported; format `std::to_underlying(value)` explicitly for numbers.
Invalid literal specifications fail compile-time format checking; dynamic formats throw
`std::format_error`.
