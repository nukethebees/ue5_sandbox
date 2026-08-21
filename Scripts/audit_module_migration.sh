#!/usr/bin/env bash
#
# Read-only audit for moving reflected Unreal C++ types from Sandbox into game
# plugins. It reports evidence to review; it does not build, modify config,
# move assets, or create redirects.
#
# Examples:
#   ./Scripts/audit_module_migration.sh
#   ./Scripts/audit_module_migration.sh --baseline a880ba600^

set -euo pipefail

old_module=Sandbox
baseline=HEAD
plugin_modules=(ShooterGame SandboxGameShared)

usage() {
    printf '%s\n' 'Usage: Scripts/audit_module_migration.sh [--baseline <revision>] [--old-module <module>] [--plugin-module <module>]'
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --baseline)
            baseline=$2
            shift 2
            ;;
        --old-module)
            old_module=$2
            shift 2
            ;;
        --plugin-module)
            plugin_modules+=("$2")
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            usage >&2
            exit 2
            ;;
    esac
done

repository_root=$(git rev-parse --show-toplevel)
cd "$repository_root"

section() {
    printf '\n=== %s ===\n' "$1"
}

# Emits Kind<TAB>C++Name. UINTERFACE is a reflected class; the corresponding
# I-prefixed pure C++ interface deliberately does not require a redirect.
reflected_types() {
    perl -0777 -ne '
        while (/U(?:CLASS|INTERFACE)(?:\s*\([^)]*\))?\s*class\s+(?:[A-Za-z_][A-Za-z0-9_]*_API\s+)?([AU][A-Za-z0-9_]*)/g) { print "Class\t$1\n"; }
        while (/USTRUCT(?:\s*\([^)]*\))?\s*struct\s+(?:[A-Za-z_][A-Za-z0-9_]*_API\s+)?(F[A-Za-z0-9_]*)/g) { print "Struct\t$1\n"; }
        while (/UENUM(?:\s*\([^)]*\))?\s*enum(?:\s+class)?\s+(E[A-Za-z0-9_]*)/g) { print "Enum\t$1\n"; }
    '
}

reflection_name() {
    local kind=$1
    local cpp_name=$2
    case "$kind:$cpp_name" in
        Class:A*|Class:U*) printf '%s' "${cpp_name:1}" ;;
        Struct:F*) printf '%s' "${cpp_name:1}" ;;
        *) printf '%s' "$cpp_name" ;;
    esac
}

redirect_exists() {
    local kind=$1
    local old_name=$2
    local new_name=$3
    rg -q -F "+${kind}Redirects=(OldName=\"$old_name\",NewName=\"$new_name\")" Config/DefaultEngine.ini
}

plugin_module_for_path() {
    local path=$1
    local module
    for module in "${plugin_modules[@]}"; do
        if [[ $path == "Plugins/$module/Source/$module/"* ]]; then
            printf '%s' "$module"
            return 0
        fi
    done
    return 1
}

section 'Moved reflected types and expected redirects'
rename_count=0
migration_paths=("Source/$old_module")
for module in "${plugin_modules[@]}"; do
    migration_paths+=("Plugins/$module")
done

while IFS=$'\t' read -r status old_path new_path; do
    [[ $status == R* && $old_path == Source/* ]] || continue
    module=$(plugin_module_for_path "$new_path") || continue
    ((rename_count += 1))

    while IFS=$'\t' read -r kind cpp_name; do
        [[ -n ${kind:-} ]] || continue
        reflection=$(reflection_name "$kind" "$cpp_name")
        old_path_name="/Script/$old_module.$reflection"
        new_path_name="/Script/$module.$reflection"

        # A matching type in the new header proves this is a module move, not
        # merely a renamed header containing unrelated reflected types.
        if ! reflected_types < "$new_path" | awk -F $'\t' -v kind="$kind" -v name="$cpp_name" '$1 == kind && $2 == name { found = 1 } END { exit !found }'; then
            continue
        fi

        if redirect_exists "$kind" "$old_path_name" "$new_path_name"; then
            printf 'OK      %s: %s -> %s\n' "$kind" "$old_path_name" "$new_path_name"
        else
            printf 'MISSING %s: %s -> %s\n' "$kind" "$old_path_name" "$new_path_name"
        fi
    done < <(git show "$baseline:$old_path" 2>/dev/null | reflected_types)
done < <(git -c core.safecrlf=false diff --name-status --find-renames=20% "$baseline" -- "${migration_paths[@]}" 2>/dev/null)

if [[ $rename_count -eq 0 ]]; then
    printf 'No renamed files found relative to %s. For a committed migration, pass its parent revision with --baseline.\n' "$baseline"
fi

section 'Old-module references in config and assets'
old_script_path="/Script/$old_module."
if ! rg -a -l --glob '!Binaries/**' --glob '!Intermediate/**' --glob '!DerivedDataCache/**' --fixed-strings "$old_script_path" Config Content Plugins; then
    printf 'No readable references to %s found.\n' "$old_script_path"
fi

section 'Stale includes that have a plugin header counterpart'
declare -A headers=()
for module in "${plugin_modules[@]}"; do
    public_root="Plugins/$module/Source/$module/Public/$module"
    [[ -d $public_root ]] || continue
    while IFS= read -r header; do
        relative_header=${header#"$public_root/"}
        headers["$relative_header"]=$module
    done < <(find "$public_root" -type f \( -name '*.h' -o -name '*.hpp' \))
done

stale_include_count=0
while IFS= read -r match; do
    include_path=$(sed -n 's/.*[<"]Sandbox\/\([^>"]*\)[>"]$/\1/p' <<< "$match")
    [[ -n $include_path && -n ${headers[$include_path]:-} ]] || continue
    printf 'STALE   %s -> %s/%s\n' "$match" "${headers[$include_path]}" "$include_path"
    ((stale_include_count += 1))
done < <(rg -n '#include [<"]Sandbox/' Source Plugins --glob '*.{h,hpp,cpp}' || true)

if [[ $stale_include_count -eq 0 ]]; then
    printf '%s\n' 'No stale Sandbox includes with a plugin header counterpart found.'
fi

section 'Quoted relative includes in plugin implementation files'
relative_include_count=0
for module in "${plugin_modules[@]}"; do
    private_root="Plugins/$module/Source/$module/Private"
    [[ -d $private_root ]] || continue
    while IFS= read -r match; do
        printf 'RELATIVE %s\n' "$match"
        ((relative_include_count += 1))
    done < <(rg -n '^\s*#include\s+"[^/"]+"' "$private_root" --glob '*.cpp' || true)
done

if [[ $relative_include_count -eq 0 ]]; then
    printf '%s\n' 'No quoted relative includes found in plugin .cpp files.'
fi

printf '\nAudit complete. Review findings before changing redirects or assets.\n'
