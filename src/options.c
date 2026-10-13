#define _POSIX_C_SOURCE 200809L
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "options.h"

/* Parse only the switches listed in the supplied NetBSD ls(1) handout. */
void options_init(Options *options)
{
    memset(options, 0, sizeof(*options));
    options->sort_mode = SORT_BY_NAME;
    options->time_mode = TIME_MODIFIED;
    options->long_mode = LONG_DISABLED;
    options->size_mode = SIZE_DEFAULT_BLOCKS;
    options->name_mode = NAMES_AUTO;

    /* NetBSD ls -A behavior: it is enabled by default for the super-user. */
    if (geteuid() == 0) {
        options->show_hidden = true;
    }
}

void print_usage(const char *program_name)
{
    fprintf(stderr,
            "Usage: %s [-AacdFfhiklnqRrSstuw] [file ...]\n",
            program_name);
}

int parse_options(int argc, char **argv, Options *options, int *first_operand)
{
    int option;

    opterr = 0;
    optind = 1;
    while ((option = getopt(argc, argv, "AacdFfhiklnqRrSstuw")) != -1) {
        /* Assignment order intentionally implements last-option-wins pairs. */
        switch (option) {
        case 'A':
            options->show_hidden = true;
            break;
        case 'a':
            options->show_hidden = true;
            options->show_dot_and_dotdot = true;
            break;
        case 'c':
            options->time_mode = TIME_CHANGED;
            break;
        case 'd':
            options->directory_as_file = true;
            options->recursive = false;
            break;
        case 'F':
            options->classify = true;
            break;
        case 'f':
            options->no_sort = true;
            break;
        case 'h':
            options->size_mode = SIZE_HUMAN_READABLE;
            break;
        case 'i':
            options->show_inode = true;
            break;
        case 'k':
            options->size_mode = SIZE_KILOBYTES;
            break;
        case 'l':
            options->long_mode = LONG_OWNER_NAMES;
            break;
        case 'n':
            options->long_mode = LONG_NUMERIC_IDS;
            break;
        case 'q':
            options->name_mode = NAMES_QUOTED;
            break;
        case 'R':
            options->recursive = true;
            options->directory_as_file = false;
            break;
        case 'r':
            options->reverse = true;
            break;
        case 'S':
            options->sort_mode = SORT_BY_SIZE;
            break;
        case 's':
            options->show_blocks = true;
            break;
        case 't':
            options->sort_mode = SORT_BY_TIME;
            break;
        case 'u':
            options->time_mode = TIME_ACCESSED;
            break;
        case 'w':
            options->name_mode = NAMES_RAW;
            break;
        case '?':
        default:
            if (optopt != 0) {
                fprintf(stderr, "%s: illegal option -- %c\n", argv[0], optopt);
            } else {
                fprintf(stderr, "%s: invalid option\n", argv[0]);
            }
            print_usage(argv[0]);
            return -1;
        }
    }

    *first_operand = optind;
    return 0;
}
