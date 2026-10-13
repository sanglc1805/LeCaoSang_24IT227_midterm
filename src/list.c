#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "list.h"
#include "display.h"
#include "sort.h"

/* EntryList owns copies of both strings, so readdir() buffer reuse is safe. */
void entry_list_init(EntryList *list)
{
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void entry_list_destroy(EntryList *list)
{
    size_t index;
    for (index = 0; index < list->count; ++index) {
        free(list->items[index].name);
        free(list->items[index].path);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

int entry_list_add(EntryList *list, const char *name, const char *path,
                   const struct stat *st, bool stat_ok)
{
    Entry *new_items;
    size_t new_capacity;
    Entry entry;

    if (list->count == list->capacity) {
        new_capacity = list->capacity == 0 ? 16 : list->capacity * 2;
        if (new_capacity < list->capacity ||
            new_capacity > (size_t)-1 / sizeof(*list->items)) {
            errno = ENOMEM;
            return -1;
        }
        new_items = realloc(list->items, new_capacity * sizeof(*list->items));
        if (new_items == NULL) {
            return -1;
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }

    entry.name = strdup(name);
    entry.path = strdup(path);
    if (entry.name == NULL || entry.path == NULL) {
        free(entry.name);
        free(entry.path);
        return -1;
    }
    if (stat_ok && st != NULL) {
        entry.st = *st;
    } else {
        memset(&entry.st, 0, sizeof(entry.st));
    }
    entry.stat_ok = stat_ok;
    list->items[list->count++] = entry;
    return 0;
}

static char *join_path(const char *directory, const char *name)
{
    size_t directory_length = strlen(directory);
    size_t name_length = strlen(name);
    bool needs_slash = directory_length > 0 && directory[directory_length - 1] != '/';
    char *result;

    if (directory_length > (size_t)-1 - name_length - (needs_slash ? 2U : 1U)) {
        errno = ENOMEM;
        return NULL;
    }
    result = malloc(directory_length + name_length + (needs_slash ? 2U : 1U));
    if (result == NULL) {
        return NULL;
    }
    memcpy(result, directory, directory_length);
    if (needs_slash) result[directory_length++] = '/';
    memcpy(result + directory_length, name, name_length);
    result[directory_length + name_length] = '\0';
    return result;
}

static bool should_show_name(const char *name, const Options *options)
{
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return options->show_dot_and_dotdot;
    }
    if (name[0] == '.' && !options->show_hidden) {
        return false;
    }
    return true;
}

/* Read one directory; lstat() keeps links inside a listing identified as links. */
int list_directory(const char *path, const Options *options, EntryList *entries)
{
    DIR *directory;
    struct dirent *item;
    int status = 0;

    entry_list_init(entries);
    directory = opendir(path);
    if (directory == NULL) {
        perror(path);
        return -1;
    }

    for (;;) {
        char *child_path;
        struct stat st;
        bool stat_ok;

        errno = 0;
        item = readdir(directory);
        if (item == NULL) {
            if (errno != 0) {
                perror(path);
                status = -1;
            }
            break;
        }
        if (!should_show_name(item->d_name, options)) {
            continue;
        }

        child_path = join_path(path, item->d_name);
        if (child_path == NULL) {
            perror("myls: allocate path");
            status = -1;
            break;
        }
        stat_ok = lstat(child_path, &st) == 0;
        if (!stat_ok) {
            perror(child_path);
            status = -1;
        }
        if (entry_list_add(entries, item->d_name, child_path,
                           stat_ok ? &st : NULL, stat_ok) == -1) {
            perror("myls: allocate entry");
            free(child_path);
            status = -1;
            break;
        }
        free(child_path);
    }

    if (closedir(directory) == -1) {
        perror(path);
        status = -1;
    }
    return status;
}

static int compare_directory_paths(const void *left_pointer, const void *right_pointer)
{
    const char *const *left = left_pointer;
    const char *const *right = right_pointer;
    return strcmp(*left, *right);
}

static int append_directory(char ***directories, size_t *count, size_t *capacity,
                            const char *path)
{
    char **larger;
    char *copy;
    size_t new_capacity;

    if (*count == *capacity) {
        new_capacity = *capacity == 0 ? 8 : *capacity * 2;
        if (new_capacity < *capacity || new_capacity > (size_t)-1 / sizeof(**directories)) {
            errno = ENOMEM;
            return -1;
        }
        larger = realloc(*directories, new_capacity * sizeof(**directories));
        if (larger == NULL) return -1;
        *directories = larger;
        *capacity = new_capacity;
    }
    copy = strdup(path);
    if (copy == NULL) return -1;
    (*directories)[(*count)++] = copy;
    return 0;
}


/* -d must not follow a symlink operand, even if the argument ends in '/'. */
static char *copy_without_trailing_slashes(const char *path)
{
    size_t length = strlen(path);
    char *copy = malloc(length + 1);
    if (copy == NULL) return NULL;
    memcpy(copy, path, length + 1);
    while (length > 1 && copy[length - 1] == '/') {
        copy[--length] = '\0';
    }
    return copy;
}

static bool is_dot_or_dotdot(const char *name)
{
    return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

/* List a directory and, for -R, recurse only into real directory entries. */
static int list_directory_tree(const char *path, const Options *options,
                               bool show_header, bool *printed_directory)
{
    EntryList entries;
    size_t index;
    int status = 0;
    bool show_total = options->long_mode != LONG_DISABLED ||
                      (options->show_blocks && isatty(STDOUT_FILENO));

    if (show_header) {
        if (*printed_directory) (void)putchar('\n');
        display_directory_heading(path, options);
        *printed_directory = true;
    }

    if (list_directory(path, options, &entries) == -1) {
        status = -1;
    }
    sort_entries(&entries, options);
    display_entries(&entries, options, show_total);

    if (options->recursive) {
        for (index = 0; index < entries.count; ++index) {
            const Entry *entry = &entries.items[index];
            if (entry->stat_ok && S_ISDIR(entry->st.st_mode) &&
                !is_dot_or_dotdot(entry->name)) {
                if (list_directory_tree(entry->path, options, true,
                                        printed_directory) == -1) {
                    status = -1;
                }
            }
        }
    }

    entry_list_destroy(&entries);
    return status;
}

/* Keep file operands and directory operands separate as required by the manual. */
int run_listing(const Options *options, int argc, char **argv, int first_operand)
{
    EntryList files;
    char **directories = NULL;
    size_t directory_count = 0, directory_capacity = 0;
    size_t index;
    int status = 0;
    int operand_count = argc - first_operand;
    bool printed_directory = false;
    bool show_headers;

    entry_list_init(&files);
    if (operand_count == 0) {
        static char *default_operand[] = {"."};
        argv = default_operand;
        first_operand = 0;
        operand_count = 1;
    }

    for (index = 0; index < (size_t)operand_count; ++index) {
        const char *path = argv[first_operand + (int)index];
        struct stat target_stat;
        struct stat link_stat;
        bool is_directory = false;
        char *no_slash_path = NULL;
        const char *lookup_path = path;

        if (!options->directory_as_file && stat(path, &target_stat) == 0 &&
            S_ISDIR(target_stat.st_mode)) {
            is_directory = true;
        }

        if (is_directory) {
            if (append_directory(&directories, &directory_count,
                                 &directory_capacity, path) == -1) {
                perror("myls: allocate directory list");
                status = 1;
            }
            continue;
        }

        if (options->directory_as_file) {
            no_slash_path = copy_without_trailing_slashes(path);
            if (no_slash_path == NULL) {
                perror("myls: allocate operand path");
                status = 1;
                continue;
            }
            lookup_path = no_slash_path;
        }
        if (lstat(lookup_path, &link_stat) == -1) {
            perror(path);
            free(no_slash_path);
            status = 1;
            continue;
        }
        if (entry_list_add(&files, path, lookup_path, &link_stat, true) == -1) {
            perror("myls: allocate file list");
            free(no_slash_path);
            status = 1;
            break;
        }
        free(no_slash_path);
    }

    if (!options->no_sort) {
        sort_operand_entries(&files, options->reverse);
        if (directory_count > 1) {
            qsort(directories, directory_count, sizeof(directories[0]),
                  compare_directory_paths);
            if (options->reverse) {
                size_t left = 0, right = directory_count - 1;
                while (left < right) {
                    char *temporary = directories[left];
                    directories[left] = directories[right];
                    directories[right] = temporary;
                    ++left;
                    --right;
                }
            }
        }
    }

    if (files.count > 0) {
        display_entries(&files, options, false);
    }

    show_headers = operand_count > 1 || options->recursive;
    if (files.count > 0 && directory_count > 0) {
        (void)putchar('\n');
    }
    for (index = 0; index < directory_count; ++index) {
        if (list_directory_tree(directories[index], options, show_headers,
                                &printed_directory) == -1) {
            status = 1;
        }
    }

    entry_list_destroy(&files);
    for (index = 0; index < directory_count; ++index) {
        free(directories[index]);
    }
    free(directories);
    if (ferror(stdout)) status = 1;
    return status;
}
