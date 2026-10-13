#ifndef SORT_H
#define SORT_H

#include "ls.h"

void sort_entries(EntryList *list, const Options *options);
void sort_operand_entries(EntryList *list, bool reverse);

#endif
