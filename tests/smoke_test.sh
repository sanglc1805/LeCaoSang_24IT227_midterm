#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM

mkdir -p "$TMP/dir/subdir" "$TMP/sortdir"
printf 'hello\n' > "$TMP/dir/alpha.txt"
printf 'world\n' > "$TMP/dir/subdir/beta.txt"
printf 'hidden\n' > "$TMP/dir/.hidden"
printf 'small\n' > "$TMP/sortdir/small.txt"
printf 'this is the larger fixture file\n' > "$TMP/sortdir/large.txt"
BAD_NAME=$(printf 'bad\nname')
printf 'line one\nline two\n' > "$TMP/dir/$BAD_NAME"
chmod +x "$TMP/dir/alpha.txt"
ln -s subdir "$TMP/dir/linkdir"

# Default listing and the super-user's automatic -A behavior.
OUT=$($ROOT/myls "$TMP/dir")
printf '%s\n' "$OUT" | grep -Fxq 'alpha.txt'
if [ "$(id -u)" -ne 0 ] && printf '%s\n' "$OUT" | grep -Fxq '.hidden'; then
    echo 'default listing unexpectedly showed hidden file for a normal user' >&2
    exit 1
fi

# -A includes hidden names but not the special . and .. entries.
OUT=$($ROOT/myls -A "$TMP/dir")
printf '%s\n' "$OUT" | grep -Fxq '.hidden'
if printf '%s\n' "$OUT" | grep -Fxq '.' || printf '%s\n' "$OUT" | grep -Fxq '..'; then
    echo '-A should not show . or ..' >&2
    exit 1
fi

# -a includes . and .. as well as hidden files.
OUT=$($ROOT/myls -a "$TMP/dir")
printf '%s\n' "$OUT" | grep -Fxq '.hidden'
printf '%s\n' "$OUT" | grep -Fxq '.'
printf '%s\n' "$OUT" | grep -Fxq '..'

# -F marks directories, executable files, and symbolic links.
OUT=$($ROOT/myls -F "$TMP/dir")
printf '%s\n' "$OUT" | grep -Fxq 'subdir/'
printf '%s\n' "$OUT" | grep -Fxq 'alpha.txt*'
printf '%s\n' "$OUT" | grep -Fxq 'linkdir@'

# Long format, inode, blocks, and numeric owner/group IDs.
OUT=$($ROOT/myls -lh "$TMP/dir")
printf '%s\n' "$OUT" | grep -q '^total '
printf '%s\n' "$OUT" | grep -q 'alpha.txt'
OUT=$($ROOT/myls -i "$TMP/dir")
printf '%s\n' "$OUT" | grep -Eq '^[0-9]+ '
OUT=$($ROOT/myls -s "$TMP/dir")
printf '%s\n' "$OUT" | grep -Eq '^[0-9]+ '
OUT=$($ROOT/myls -n "$TMP/dir/alpha.txt")
printf '%s\n' "$OUT" | grep -Eq '^[^ ]{10} +[0-9]+ +[0-9]+ '

# -q masks a newline in the file name; ordinary printable names stay readable.
OUT=$($ROOT/myls -q "$TMP/dir")
printf '%s\n' "$OUT" | grep -Fxq 'bad?name'

# Sorting by size, reverse order, and recursive listing.
OUT=$($ROOT/myls -S "$TMP/sortdir")
[ "$(printf '%s\n' "$OUT" | head -n 1)" = 'large.txt' ]
OUT=$($ROOT/myls -r "$TMP/sortdir")
[ "$(printf '%s\n' "$OUT" | head -n 1)" = 'small.txt' ]
OUT=$($ROOT/myls -R "$TMP/dir")
printf '%s\n' "$OUT" | grep -Fq "$TMP/dir/subdir:"
if printf '%s\n' "$OUT" | grep -Fq "$TMP/dir/linkdir:"; then
    echo '-R should not recurse through a symlink found inside a directory' >&2
    exit 1
fi
printf '%s\n' "$OUT" | grep -Fxq 'beta.txt'

# -d treats the directory operand as a plain entry; -d and -R are last-option-wins.
OUT=$($ROOT/myls -d "$TMP/dir")
[ "$OUT" = "$TMP/dir" ]
OUT=$($ROOT/myls -R -d "$TMP/dir")
[ "$OUT" = "$TMP/dir" ]
OUT=$($ROOT/myls -d -R "$TMP/dir")
printf '%s\n' "$OUT" | grep -Fq "$TMP/dir/subdir:"

# Multiple operands and invalid paths.
OUT=$($ROOT/myls "$TMP/dir/alpha.txt" "$TMP/sortdir")
printf '%s\n' "$OUT" | grep -q 'alpha.txt'
printf '%s\n' "$OUT" | grep -Fq "$TMP/sortdir:"
if $ROOT/myls "$TMP/no-such-path" >/dev/null 2>&1; then
    echo 'non-existent path should return non-zero' >&2
    exit 1
fi
if $ROOT/myls -z >/dev/null 2>&1; then
    echo 'unknown option should return non-zero' >&2
    exit 1
fi

echo 'Smoke tests passed.'
