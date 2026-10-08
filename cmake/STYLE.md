# CMake style

- Declare targets first; list their files with `target_sources`.
- Put `PUBLIC`, `PRIVATE`, and `INTERFACE` on their own lines, with arguments indented beneath them.
- Use multiline calls for source, dependency, and option lists; put the closing parenthesis on its own line.
- Name repeated or long paths with local variables. Keep output layouts consistent with Unreal consumers.
- Extract repeated command sequences into small functions with explicit parameters.
- Prefer target-scoped settings and explicit usage requirements.
- Keep benchmark and test target setup in their own subdirectories.
