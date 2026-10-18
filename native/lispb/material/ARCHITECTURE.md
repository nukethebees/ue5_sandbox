# MaterialGen architecture

MaterialGen parses a constrained material graph language, validates node and connection semantics,
and emits Unreal material assets through the repository generator. It keeps graph layout, commands,
and backend-specific asset construction explicit so generated changes can be reviewed as source.

The Unreal 5.8 backend owns translation from supported graph constructs to engine material
expressions. It does not provide arbitrary engine-object scripting: unsupported nodes or invalid
connections fail generation. Material source remains the authority; generated assets are refreshed
only through the matching CMake target.

See [README.md](README.md) for usage.

The compiler supplies shared source and syntax stores to material analysis. `MaterialIR` is the
exported runtime boundary: the Unreal editor deserializes it without linking the compiler frontend,
so it owns its flat string storage and graph. Names, paths, and HLSL reference string handles;
node kinds, types, source locations, and input ranges occupy central columns, with separate payload
columns for each kind. Four-byte node handles retain source-order indices used by the binary format.
Callers retain node handles and read only the required columns. Kind-specific creation
functions publish payloads directly into the central stores.
Parameters, named bindings/outputs, and custom input declarations use separate plural stores
and four-byte IDs. Ordered node input declarations use flat handle ranges; no material node or
declaration access object is reconstructed.

Serialization retains the version 6 field order and encodings. Kind-specific storage reconstructs
unused fields as their format defaults, and custom input edges are written from the ordinary input
range. Validation stops after semantic analysis and asset dependency checks, before debug rendering,
hashing, or serialization.
