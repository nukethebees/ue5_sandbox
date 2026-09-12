import unittest

from Scripts import sbx_mimalloc_symbols as symbols


class MimallocSymbolTests(unittest.TestCase):
    def test_extracts_c_cpp_and_internal_identifiers(self) -> None:
        output = """
00000000 T mi_malloc
00000000 T void __cdecl _mi_heap_init(struct mi_heap_s *)
00000000 R _mi_tls_callback_pre
00000000 T sbx_mi_free
"""
        self.assertEqual(
            symbols.allocator_identifiers(output),
            {"mi_malloc", "_mi_heap_init", "mi_heap_s", "_mi_tls_callback_pre"},
        )

    def test_extracts_linker_includes(self) -> None:
        directives = " /INCLUDE:_tls_used /INCLUDE:_mi_tls_callback_pre"
        self.assertEqual(
            symbols.linker_includes(directives), {"_tls_used", "_mi_tls_callback_pre"}
        )

    def test_ignores_msvc_string_literal_symbols(self) -> None:
        output = """
00000000 R ??_C@_07LDBHFKDL@mi_free?$AA@
00000000 T mi_free
"""
        self.assertEqual(symbols.without_msvc_string_literals(output).strip(), "00000000 T mi_free")

    def test_detects_all_msvc_operator_new_delete_forms(self) -> None:
        decorated = ["??2@", "??3@", "??_U@", "??_V@"]
        self.assertTrue(all(symbols.MSVC_OPERATOR_NEW_DELETE.match(name) for name in decorated))
        self.assertIsNone(symbols.MSVC_OPERATOR_NEW_DELETE.match("??_C@"))

    def test_header_prefixes_symbols_and_forces_private_callback(self) -> None:
        header = symbols.render_header(
            {"mi_malloc", "_mi_tls_callback_pre"}, {"_mi_tls_callback_pre", "_tls_used"}
        )
        self.assertIn("#define mi_malloc sbx_mi_malloc", header)
        self.assertIn("#define _mi_tls_callback_pre sbx__mi_tls_callback_pre", header)
        self.assertIn('/include:sbx__mi_tls_callback_pre', header)
        self.assertNotIn("/alternatename:", header)
        self.assertNotIn("/include:sbx__tls_used", header)


if __name__ == "__main__":
    unittest.main()
