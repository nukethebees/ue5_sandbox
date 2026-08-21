# Migration audit scripts

`audit_module_migration.sh` is a read-only audit for moving reflected Unreal C++
types from `Sandbox` into game plugins. It is intended to make follow-up module
migrations safer and repeatable.

Run it from the repository root:

```bash
./Scripts/audit_module_migration.sh
```

The default comparison is the working tree against `HEAD`. For a migration that
has already been committed, provide the commit immediately before that migration:

```bash
./Scripts/audit_module_migration.sh --baseline <migration-parent-commit>
```

The script reports expected class, struct, and enum Core Redirects for renamed
headers, readable old-module script references, stale includes, and quoted
relative includes in plugin `.cpp` files. Its output is advisory: do not add a
redirect merely because a type is reported. Confirm that the reflected type was
actually moved and that its serialized identity changed.

Do not make this script build Unreal, modify config, rewrite source, or move
content assets. Those operations need explicit review because redirect and asset
migration decisions can affect existing maps and Blueprints.
