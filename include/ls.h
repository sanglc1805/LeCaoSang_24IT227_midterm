#ifndef LS_H
#define LS_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>

typedef enum {
    SORT_BY_NAME,
    SORT_BY_SIZE,
    SORT_BY_TIME
} SortMode;

typedef enum {
    TIME_MODIFIED,
    TIME_CHANGED,
    TIME_ACCESSED
} TimeMode;

typedef enum {
    LONG_DISABLED,
    LONG_OWNER_NAMES,
    LONG_NUMERIC_IDS
} LongMode;

typedef enum {
    SIZE_DEFAULT_BLOCKS,
    SIZE_KILOBYTES,
    SIZE_HUMAN_READABLE
} SizeMode;

typedef enum {
    NAMES_AUTO,
    NAMES_QUOTED,
    NAMES_RAW
} NameMode;

typedef struct {
    bool show_hidden;          /* -a or -A */
    bool show_dot_and_dotdot;  /* -a */
    bool directory_as_file;    /* -d */
    bool recursive;            /* -R */
    bool classify;              /* -F */
    bool no_sort;               /* -f */
    bool reverse;               /* -r */
    bool show_inode;            /* -i */
    bool show_blocks;           /* -s */
    LongMode long_mode;         /* -l / -n; last one wins */
    SortMode sort_mode;         /* -S / -t; last one wins */
    TimeMode time_mode;         /* -c / -u; last one wins */
    SizeMode size_mode;         /* -h / -k; last one wins */
    NameMode name_mode;         /* -q / -w; last one wins */
} Options;

typedef struct {
    char *name;                 /* Name shown to the user */
    char *path;                 /* Path used to query the file system */
    struct stat st;
    bool stat_ok;
} Entry;

typedef struct {
    Entry *items;
    size_t count;
    size_t capacity;
} EntryList;

#endif
