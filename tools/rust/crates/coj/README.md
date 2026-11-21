# coj

Install the CLI using the [worktree setup](../../../../docs/build-and-test.md#setup). Use `coj --help` to list commands and `coj git --help` for supported Git operations.

Start an independent change with `coj git switch -c feature/<task> dev`. Continue related changes on their feature branch. `coj git check` previews a supported action. `dev`, `main`, `master`, and branches owned by other worktrees are protected.

After validation and explicit maintainer approval, `coj integrate` rebases the clean feature branch, merges it into `dev`, and removes the branch. Pass `--keep-branch` to retain it.
