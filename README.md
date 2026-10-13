# Midterm Project — Simplified `ls(1)`

**Student:** Sang Lê  
**Student ID:** 24IT227  

A modular C implementation of a subset of the NetBSD `ls(1)` manual supplied for the midterm project. It uses POSIX directory and file-status interfaces and is intended for Linux/Ubuntu.

## Implemented options

- `-A`: show hidden entries except `.` and `..`; enabled by default when run as the super-user.
- `-a`: show entries including `.` and `..`.
- `-c`: use status-change time for `-t` sorting and `-l` display.
- `-d`: list directory operands as entries rather than reading their contents; do not follow directory symlink operands.
- `-F`: append common file-type marks (`/`, `*`, `@`, `=`, `|`; `%` where the platform exposes whiteout files).
- `-f`: preserve directory-read order instead of sorting.
- `-h`: show sizes in human-readable units for `-s` and `-l`; overrides `-k` when it appears later in the option string.
- `-i`: show inode numbers.
- `-k`: show `-s` allocation counts in KiB; overrides an earlier `-h` for block counts.
- `-l`: long listing with file mode, links, owner/group names, size, time, and name.
- `-n`: like `-l`, but display numeric user and group IDs. The last of `-l` and `-n` wins.
- `-q`: replace non-printable filename characters with `?`.
- `-R`: recursively list subdirectories. The last of `-R` and `-d` wins.
- `-r`: reverse the selected sort order.
- `-S`: sort by file size, largest first.
- `-s`: show allocated block counts.
- `-t`: sort by selected time, newest first.
- `-u`: use access time for `-t` sorting and `-l` display.
- `-w`: print filename bytes without replacing non-printable characters; the last of `-q` and `-w` wins.

With no path arguments, the current directory is listed. File operands are displayed before directory operands. Multiple directory operands are processed in lexical order. Options may be combined, for example `./myls -la`.

### Platform note

The supplied manual is for NetBSD, while this project is developed on Linux/Ubuntu. Linux does not expose NetBSD whiteout entries through the standard POSIX interfaces; therefore `%` is only emitted on a platform whose headers provide `S_ISWHT`. `-s` honors a valid `BLOCKSIZE` value when neither `-h` nor `-k` overrides it. Terminal-specific output behavior is determined using `isatty()`.

## Build

Requirements: GCC (or another C compiler), GNU Make, and POSIX-compatible filesystem interfaces.

```bash
make
```

The executable is named `myls`.

## Run

```bash
./myls
./myls /etc
./myls -a
./myls -Al
./myls -li /etc
./myls -F
./myls -R src
./myls -S src
./myls -tr src
./myls -n /etc/passwd
```

Clean generated files:

```bash
make clean
```

Run the smoke tests:

```bash
make test
```

## Source layout

```text
include/       Shared declarations and module headers
src/main.c     Program entry point
src/options.c  Command-line option handling
src/list.c    Directory reading, operands, recursion, and entry storage
src/sort.c   Sorting by name, size, or selected time
src/display.c Formatting, filename display, and long listing
Makefile      Build and clean rules
README.md     Project report and usage instructions
.gitignore    Excludes generated binaries and object files
```

## Known limitations

This is a student implementation of the supplied option subset, not a full replacement for every NetBSD or GNU `ls` behavior. Terminal column layout and every platform-specific file type are not reproduced. Validate all options and combinations against the exact grading expectations and the supplied manual before submission.

## GitHub repository

Repository name: `SangLe_24IT227_midterm`  
Repository URL: **replace this line with your actual GitHub repository URL before submitting**.
