# mimalloc provenance

`native/third_party/mimalloc` is an ordinary-file, squashed Git subtree of
[microsoft/mimalloc](https://github.com/microsoft/mimalloc) tag `v3.5.0`, upstream commit
`18b08671c9302247bfb682286e6bf3cc1773f801`. The upstream MIT license is retained in the subtree.

It was imported with:

```powershell
git subtree add --prefix=native/third_party/mimalloc https://github.com/microsoft/mimalloc.git v3.5.0 --squash
```

Update it with the equivalent `git subtree pull --prefix=... --squash` command. The private build
generates its symbol-prefix header from an unprefixed probe object and audits the production object;
an upstream symbol or Windows initialization change must pass that audit before the update is used.
The production object suppresses upstream's literal linker directives while the generated header
re-emits their prefixed equivalents from the wrapper object. No vendored upstream file is patched.
