# IOJ clang-tidy checks

These checks are statically linked into clang-tidy. Build and test them using the
[LLVM checker development workflow](../README.md#checker-development).
Simulation opts into policy through `native/simulation/.clang-tidy`.

See [policy semantics](ARCHITECTURE.md) for view identity, lexical boundaries,
diagnostic ownership and explicit standard-type recognition. Each check has its own
`tests/test_*.py` semantic suite; `test_simulation_policy.py` covers inheritance and
the benchmark boundary. The LLVM install script runs every suite before installation.
