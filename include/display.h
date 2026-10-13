#ifndef DISPLAY_H
#define DISPLAY_H

#include "ls.h"

void display_entries(const EntryList *list, const Options *options, bool show_total);
void display_single_entry(const Entry *entry, const Options *options);
void display_directory_heading(const char *path, const Options *options);

#endif
