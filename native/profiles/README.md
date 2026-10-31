# Native profiles

`native-profiles` owns profiles, progression, unlock decisions, save browsing and file storage.
Unreal supplies platform paths and formats localized UI text.

Profiles use version 1 JSON payloads inside checksummed envelopes. Each replacement flushes a
unique temporary file before publishing it, preserving the previous valid generation as a backup.
Managers rebase mutations under an operating-system file lock shared by independent processes.
Clone and merge operations validate complete profile sets before publishing a staged directory.

This format intentionally replaces Unreal development saves. Old `.sav` files are not imported
or read. A missing native profile index starts a fresh default profile; existing Unreal saves
remain untouched.
