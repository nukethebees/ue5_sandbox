# MaterialGen architecture

MaterialGen parses a constrained material graph language, validates node and connection semantics,
and emits Unreal material assets through the repository generator. It keeps graph layout, commands,
and backend-specific asset construction explicit so generated changes can be reviewed as source.

The Unreal 5.8 backend owns translation from supported graph constructs to engine material
expressions. It does not provide arbitrary engine-object scripting: unsupported nodes or invalid
connections fail generation. Material source remains the authority; generated assets are refreshed
only through the matching CMake target.

See [README.md](README.md) for usage.
