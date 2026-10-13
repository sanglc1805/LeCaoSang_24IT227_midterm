#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sort.h"

/* qsort() has no context parameter, so this points to the active sort settings. */
static const Options *active_options;

static time_t selected_time(const struct stat *st, TimeMode mode)
{
    switch (mode) {
    case TIME_CHANGED:
        return st->st_ctime;
    case TIME_ACCESSED:
        return st->st_atime;
    case TIME_MODIFIED:
    default:
        return st->st_mtime;
    }
}

static int compare_entries(const void *left_pointer, const void *right_pointer)
{
    const Entry *left = left_pointer;
    const Entry *right = right_pointer;
    int result = 0;

    if (left->stat_ok != right->stat_ok) {
        result = left->stat_ok ? -1 : 1;
    } else if (active_options->sort_mode == SORT_BY_SIZE &&
               left->stat_ok && right->stat_ok &&
               left->st.st_size != right->st.st_size) {
        result = left->st.st_size > right->st.st_size ? -1 : 1;
    } else if (active_options->sort_mode == SORT_BY_TIME &&
               left->stat_ok && right->stat_ok) {
        time_t left_time = selected_time(&left->st, active_options->time_mode);
        time_t right_time = selected_time(&right->st, active_options->time_mode);
        if (left_time != right_time) {
            result = left_time > right_time ? -1 : 1;
        }
    }

    if (result == 0) {
        result = strcmp(left->name, right->name);
    }
    /* Normalize before reversing so negation cannot overflow. */
    result = (result > 0) - (result < 0);
    if (active_options->reverse) {
        result = -result;
    }
    return result;
}

static int compare_names_only(const void *left_pointer, const void *right_pointer)
{
    const Entry *left = left_pointer;
    const Entry *right = right_pointer;
    return strcmp(left->name, right->name);
}

static int compare_names_only_reverse(const void *left_pointer, const void *right_pointer)
{
    int result = compare_names_only(left_pointer, right_pointer);
    return (result > 0) ? -1 : ((result < 0) ? 1 : 0);
}

/* Default to lexical order; -f preserves readdir() order. */
void sort_entries(EntryList *list, const Options *options)
{
    if (list->count < 2 || options->no_sort) {
        return;
    }
    active_options = options;
    qsort(list->items, list->count, sizeof(list->items[0]), compare_entries);
}

/* Operand names are sorted lexically, with -r reversing that order. */
void sort_operand_entries(EntryList *list, bool reverse)
{
    if (list->count > 1) {
        qsort(list->items, list->count, sizeof(list->items[0]),
              reverse ? compare_names_only_reverse : compare_names_only);
    }
}
