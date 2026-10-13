#define _POSIX_C_SOURCE 200809L
#include <locale.h>
#include <stdio.h>

#include "options.h"
#include "list.h"

/* Program entry point: configure locale, parse flags, then run the listing. */
int main(int argc, char **argv)
{
    Options options;
    int first_operand = 0;

    /* Let filename classification and timestamps follow the user locale. */
    (void)setlocale(LC_ALL, "");
    options_init(&options);
    if (parse_options(argc, argv, &options, &first_operand) != 0) {
        return 2;
    }
    return run_listing(&options, argc, argv, first_operand) == 0 ? 0 : 1;
}
