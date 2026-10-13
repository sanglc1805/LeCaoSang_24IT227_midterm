#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <inttypes.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/sysmacros.h>
#include <time.h>
#include <unistd.h>

#include "display.h"

/* Formatting helpers are kept here so directory-reading code stays separate. */
#ifndef S_ISVTX
#define S_ISVTX 01000
#endif

/* Parse common BLOCKSIZE values, falling back safely when the value is invalid. */
static unsigned long long blocksize_from_environment(void)
{
    const char *value = getenv("BLOCKSIZE");
    char *end = NULL;
    const char *start;
    unsigned long long amount;
    unsigned long long multiplier = 1;

    if (value == NULL || *value == '\0') {
        return 512;
    }
    start = value;
    while (isspace((unsigned char)*start)) ++start;
    if (*start == '-') return 512;
    errno = 0;
    amount = strtoull(value, &end, 10);
    if (errno == ERANGE || end == value || amount == 0) {
        return 512;
    }
    if (*end != '\0') {
        if (end[1] != '\0') {
            return 512;
        }
        switch (*end) {
        case 'k': case 'K': multiplier = 1024ULL; break;
        case 'm': case 'M': multiplier = 1024ULL * 1024ULL; break;
        case 'g': case 'G': multiplier = 1024ULL * 1024ULL * 1024ULL; break;
        default: return 512;
        }
    }
    if (amount > ULLONG_MAX / multiplier) {
        return 512;
    }
    return amount * multiplier;
}

static unsigned long long allocated_bytes(const struct stat *st)
{
    if (st->st_blocks <= 0) {
        return 0;
    }
    if ((uintmax_t)st->st_blocks > ULLONG_MAX / 512ULL) {
        return ULLONG_MAX;
    }
    return (unsigned long long)st->st_blocks * 512ULL;
}

static void format_human(unsigned long long bytes, char *buffer, size_t size)
{
    static const char *const units[] = {"B", "K", "M", "G", "T", "P", "E"};
    long double value = (long double)bytes;
    size_t unit = 0;

    while (value >= 1024.0L && unit + 1 < sizeof(units) / sizeof(units[0])) {
        value /= 1024.0L;
        ++unit;
    }
    if (unit == 0) {
        (void)snprintf(buffer, size, "%lluB", bytes);
    } else {
        (void)snprintf(buffer, size, "%.1Lf%s", value, units[unit]);
    }
}

static void format_block_count(unsigned long long bytes, const Options *options,
                               char *buffer, size_t size)
{
    unsigned long long unit_size;
    unsigned long long count;

    if (options->size_mode == SIZE_HUMAN_READABLE) {
        format_human(bytes, buffer, size);
        return;
    }
    if (options->size_mode == SIZE_KILOBYTES) {
        unit_size = 1024ULL;
    } else {
        unit_size = blocksize_from_environment();
    }
    count = bytes / unit_size + ((bytes % unit_size) != 0ULL);
    (void)snprintf(buffer, size, "%llu", count);
}

static time_t selected_time(const struct stat *st, TimeMode mode)
{
    switch (mode) {
    case TIME_CHANGED: return st->st_ctime;
    case TIME_ACCESSED: return st->st_atime;
    case TIME_MODIFIED:
    default: return st->st_mtime;
    }
}

static void format_file_time(const struct stat *st, const Options *options,
                             char *buffer, size_t size)
{
    time_t value = selected_time(st, options->time_mode);
    struct tm local_time;
    time_t now = time(NULL);
    double age = difftime(now, value);
    const char *format = (age > 15552000.0 || age < -15552000.0)
                             ? "%b %e  %Y"
                             : "%b %e %H:%M";

    if (localtime_r(&value, &local_time) == NULL ||
        strftime(buffer, size, format, &local_time) == 0) {
        (void)snprintf(buffer, size, "??? ?? ??:??");
    }
}

/* Build the ten-character type/permission field used by -l. */
static void mode_string(mode_t mode, char buffer[11])
{
    char type = '-';
    if (S_ISDIR(mode)) type = 'd';
    else if (S_ISLNK(mode)) type = 'l';
    else if (S_ISCHR(mode)) type = 'c';
    else if (S_ISBLK(mode)) type = 'b';
    else if (S_ISFIFO(mode)) type = 'p';
    else if (S_ISSOCK(mode)) type = 's';
#ifdef S_ISWHT
    else if (S_ISWHT(mode)) type = 'w';
#endif

    buffer[0] = type;
    buffer[1] = (mode & S_IRUSR) ? 'r' : '-';
    buffer[2] = (mode & S_IWUSR) ? 'w' : '-';
    buffer[3] = (mode & S_ISUID) ? ((mode & S_IXUSR) ? 's' : 'S')
                                 : ((mode & S_IXUSR) ? 'x' : '-');
    buffer[4] = (mode & S_IRGRP) ? 'r' : '-';
    buffer[5] = (mode & S_IWGRP) ? 'w' : '-';
    buffer[6] = (mode & S_ISGID) ? ((mode & S_IXGRP) ? 's' : 'S')
                                 : ((mode & S_IXGRP) ? 'x' : '-');
    buffer[7] = (mode & S_IROTH) ? 'r' : '-';
    buffer[8] = (mode & S_IWOTH) ? 'w' : '-';
    if (mode & S_ISVTX) {
        buffer[9] = (mode & S_IXOTH) ? 't' : 'T';
    } else {
        buffer[9] = (mode & S_IXOTH) ? 'x' : '-';
    }
    buffer[10] = '\0';
}

/* -q masks non-printable bytes; -w and non-terminal defaults preserve them. */
static void print_name(const char *name, const Options *options)
{
    NameMode mode = options->name_mode;
    const unsigned char *cursor = (const unsigned char *)name;

    if (mode == NAMES_AUTO) {
        mode = isatty(STDOUT_FILENO) ? NAMES_QUOTED : NAMES_RAW;
    }
    if (mode == NAMES_RAW) {
        (void)fputs(name, stdout);
        return;
    }
    while (*cursor != '\0') {
        if (isprint(*cursor)) {
            (void)putchar(*cursor);
        } else {
            (void)putchar('?');
        }
        ++cursor;
    }
}

void display_directory_heading(const char *path, const Options *options)
{
    print_name(path, options);
    (void)printf(":\n");
}

static char file_suffix(const Entry *entry)
{
    if (!entry->stat_ok) return '\0';
    if (S_ISDIR(entry->st.st_mode)) return '/';
    if (S_ISLNK(entry->st.st_mode)) return '@';
    if (S_ISSOCK(entry->st.st_mode)) return '=';
    if (S_ISFIFO(entry->st.st_mode)) return '|';
    if (S_ISREG(entry->st.st_mode) && (entry->st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) return '*';
#ifdef S_ISWHT
    if (S_ISWHT(entry->st.st_mode)) return '%';
#endif
    return '\0';
}

static void print_name_with_suffix(const Entry *entry, const Options *options)
{
    print_name(entry->name, options);
    if (options->classify) {
        char suffix = file_suffix(entry);
        if (suffix != '\0') (void)putchar(suffix);
    }
}

static void owner_and_group(const Entry *entry, const Options *options,
                            char *owner, size_t owner_size,
                            char *group, size_t group_size)
{
    if (!entry->stat_ok) {
        (void)snprintf(owner, owner_size, "?");
        (void)snprintf(group, group_size, "?");
        return;
    }
    if (options->long_mode == LONG_NUMERIC_IDS) {
        (void)snprintf(owner, owner_size, "%ju", (uintmax_t)entry->st.st_uid);
        (void)snprintf(group, group_size, "%ju", (uintmax_t)entry->st.st_gid);
    } else {
        struct passwd *password = getpwuid(entry->st.st_uid);
        if (password != NULL) {
            (void)snprintf(owner, owner_size, "%s", password->pw_name);
        } else {
            (void)snprintf(owner, owner_size, "%ju", (uintmax_t)entry->st.st_uid);
        }

        /* Copy the owner before looking up the group; libc may reuse lookup storage. */
        struct group *group_entry = getgrgid(entry->st.st_gid);
        if (group_entry != NULL) {
            (void)snprintf(group, group_size, "%s", group_entry->gr_name);
        } else {
            (void)snprintf(group, group_size, "%ju", (uintmax_t)entry->st.st_gid);
        }
    }
}

static char *read_link_target(const char *path, const struct stat *st)
{
    size_t capacity = 256;
    if (st->st_size > 0 && (uintmax_t)st->st_size < (uintmax_t)SIZE_MAX - 1U) {
        capacity = (size_t)st->st_size + 1U;
    }
    char *buffer;

    if (capacity < 2) capacity = 2;
    for (;;) {
        ssize_t length;
        buffer = malloc(capacity);
        if (buffer == NULL) return NULL;
        length = readlink(path, buffer, capacity - 1);
        if (length < 0) {
            free(buffer);
            return NULL;
        }
        if ((size_t)length < capacity - 1) {
            buffer[length] = '\0';
            return buffer;
        }
        free(buffer);
        if (capacity > (size_t)-1 / 2) return NULL;
        capacity *= 2;
    }
}

/* Print one operand or directory entry using the selected output options. */
void display_single_entry(const Entry *entry, const Options *options)
{
    if (options->show_inode) {
        if (entry->stat_ok) (void)printf("%ju ", (uintmax_t)entry->st.st_ino);
        else (void)printf("? ");
    }
    if (options->show_blocks) {
        char block_text[64];
        if (entry->stat_ok) {
            format_block_count(allocated_bytes(&entry->st), options, block_text, sizeof(block_text));
            (void)printf("%s ", block_text);
        } else {
            (void)printf("? ");
        }
    }

    if (options->long_mode != LONG_DISABLED) {
        char mode[11] = "??????????";
        char owner[128], group[128], size_text[64], time_text[64];
        char *link_target = NULL;
        uintmax_t links = 0;
        if (entry->stat_ok) {
            mode_string(entry->st.st_mode, mode);
            links = (uintmax_t)entry->st.st_nlink;
            owner_and_group(entry, options, owner, sizeof(owner), group, sizeof(group));
            if (S_ISCHR(entry->st.st_mode) || S_ISBLK(entry->st.st_mode)) {
                (void)snprintf(size_text, sizeof(size_text), "%u, %u",
                               major(entry->st.st_rdev), minor(entry->st.st_rdev));
            } else if (options->size_mode == SIZE_HUMAN_READABLE) {
                format_human(entry->st.st_size < 0 ? 0 : (unsigned long long)entry->st.st_size,
                             size_text, sizeof(size_text));
            } else {
                (void)snprintf(size_text, sizeof(size_text), "%jd", (intmax_t)entry->st.st_size);
            }
            format_file_time(&entry->st, options, time_text, sizeof(time_text));
        } else {
            (void)snprintf(owner, sizeof(owner), "?");
            (void)snprintf(group, sizeof(group), "?");
            (void)snprintf(size_text, sizeof(size_text), "?");
            (void)snprintf(time_text, sizeof(time_text), "??? ?? ??:??");
        }
        (void)printf("%s %3ju %-8s %-8s %8s %s ", mode, links, owner, group, size_text, time_text);
        print_name_with_suffix(entry, options);
        if (entry->stat_ok && S_ISLNK(entry->st.st_mode)) {
            link_target = read_link_target(entry->path, &entry->st);
            if (link_target != NULL) {
                (void)printf(" -> ");
                print_name(link_target, options);
                free(link_target);
            }
        }
        (void)putchar('\n');
        return;
    }

    print_name_with_suffix(entry, options);
    (void)putchar('\n');
}

void display_entries(const EntryList *list, const Options *options, bool show_total)
{
    size_t index;
    if (show_total) {
        unsigned long long total_bytes = 0;
        char total_text[64];
        for (index = 0; index < list->count; ++index) {
            if (list->items[index].stat_ok) {
                unsigned long long bytes = allocated_bytes(&list->items[index].st);
                if (ULLONG_MAX - total_bytes < bytes) {
                    total_bytes = ULLONG_MAX;
                    break;
                }
                total_bytes += bytes;
            }
        }
        Options total_options = *options;
        /* -k changes -s counts, but does not independently change -l totals. */
        if (!options->show_blocks && total_options.size_mode == SIZE_KILOBYTES) {
            total_options.size_mode = SIZE_DEFAULT_BLOCKS;
        }
        format_block_count(total_bytes, &total_options, total_text, sizeof(total_text));
        (void)printf("total %s\n", total_text);
    }
    for (index = 0; index < list->count; ++index) {
        display_single_entry(&list->items[index], options);
    }
}
