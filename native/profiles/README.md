# Native profiles

`native-profiles` owns profiles, progression, unlock decisions, save browsing and file storage.
Unreal supplies platform paths and formats localized UI text.

Profile indexes and results share schema version 3, defined by `current_save_version` in
`save_version.h`, inside checksummed JSON envelopes. Only the current version is accepted;
older and newer saves are rejected without migration or rewriting. Each replacement flushes a
unique temporary file before publishing it, preserving the previous valid generation as a backup.
Managers rebase mutations under an operating-system file lock shared by independent processes.
Clone and merge operations validate complete profile sets before publishing a staged directory.

This format intentionally replaces Unreal development saves. Old `.sav` files are not imported
or read. A missing native profile index starts a fresh default profile; existing Unreal saves
remain untouched.
