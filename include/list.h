#ifndef LIST_H
#define LIST_H

#include "ls.h"

void entry_list_init(EntryList *list);
void entry_list_destroy(EntryList *list);
int entry_list_add(EntryList *list, const char *name, const char *path,
                   const struct stat *st, bool stat_ok);
int list_directory(const char *path, const Options *options, EntryList *entries);
int run_listing(const Options *options, int argc, char **argv, int first_operand);

#endif
