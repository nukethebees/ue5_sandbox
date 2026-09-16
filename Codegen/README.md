# Generated code

`Codegen/` holds committed generated C++ and support files. Its `cpp/`, `soa/`, and `manifests/`
subdirectories are outputs, not the source of truth; change the relevant `lispb/` input or generator
instead of editing them by hand.

The `lispb` native tool generates schemas, Slate, kernels, and compiled material IR from `.lispb`
inputs. Named targets and groups are defined in `lispb/project.lispb`; schema inputs live under
`lispb/schema`. The [native lispb guide](../native/lispb/README.md) links the specialised language,
Slate, kernel, and material documentation.

Regenerate committed outputs from the repository root:

```powershell
cmake --workflow --preset generate-code
```

Verify that committed generated files are current without writing them:

```powershell
cmake --build --preset codegen --target check-generated-code
```

Use the generated-code check after changing inputs or generators. The broader CMake workflow is
documented in [Build and test](../docs/build-and-test.md).
