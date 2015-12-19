/**
 * @file myexpand.c
 * @author Constantin Schieber (1228774) <e1228774@student.tuwien.ac.at>
 * @date 14.10.2015
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>




/* === Global Variables === */

/* Name of the program */
static const char *pgm_name = "myexpand";

/**
 * @brief terminate program on program error
 * @param exitcode exit code
 * @param fmt format string
 */
static void bail_out(int exitcode, const char *fmt, ...);

/**
 * Closes all open streams.
 * @return void
 */
static void cleanup(void) 
{
    if(fcloseall() != 0) {
        bail_out(EXIT_FAILURE, "Not all streams could be closed.\n");
    }
}

/**
 * Expands tabs to spaces.
 *
 * @param fp The pointer to the input stream.
 * @param tabstop Position where the tab should end.
 * @return void
 */
static void expand (FILE *fp, int tabstop)
{
    /* p Next multiple of tabstop.
     * x Current index in line.
     * i Helper for counting.
     * c Current character from input stream.
     */
    int p, x = 0, i;
    char c;

    while((c = fgetc(fp)) != EOF) {
        if(c == '\t') {
            p = tabstop * ((x / tabstop) + 1);
//            printf("Tabstop t: %d, p: %d, x: %d\n", tabstop, p, x);
            /* Insert spaces. */
            for(i = 0; i < (p-x); i++) {
                fprintf(stdout, " ");
            }
            x = p;
        }
        else {
            if( fputc(c, stdout) == EOF) {
                bail_out(EXIT_FAILURE, "Couldn't write to stdout.\n");
            }
            /* Check for NewLine. */ 
            if(c == '\n') {
                x = 0;
            } else {
                x++;
            }
        }
    }
}

/**
 * The main entry point of the program.
 * 
 * @param argc The number of command-line parameters in argv.
 * @param argv The array of command-line paramters, argc elements long.
 * @return The exit code of the program. 0 on success, non-zero on failure.
 */
int main(int argc, char** argv)
{
    int opt, tabs=8, i = 0;
    FILE *fp;
    
    (void) atexit (cleanup);

    /* Set the name of the program */
    pgm_name = argv[0];

    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
        case 't':
            if(optarg && isdigit(*optarg)) {
                tabs = strtol(optarg, NULL, 10); /* Convert char* to number */
                if ((errno == ERANGE &&
                        (tabs == LONG_MAX || tabs == LONG_MIN))
                    || (errno != 0 && tabs == 0)) {
                    bail_out(EXIT_FAILURE, "Converting tabstop to int failed.\n");
                }
            } else {
                 bail_out(EXIT_FAILURE, "Usage: %s [-t tabstop] [file ...]\n", pgm_name);
            }
            break;
        default: /* '?' */
           bail_out(EXIT_FAILURE, "Usage: %s [-t tabstop] [file ...]\n", pgm_name);
        }
    }
    
    /* IF optind is smaller than argc parse on ELSE use stdin */
    if(optind < argc) {
        //printf("optind < argc\n");
        for(i = (optind); i < argc; i++) {
            //printf("ARGV [%i]: %s\n", i, argv[i]);
            if( ((fp = fopen((char*)(argv[i]), "r")) == NULL)) {
               bail_out(EXIT_FAILURE, "Error opening file.\n"); 
            }
            (void)expand(fp, tabs);
            if( (fclose(fp) == EOF )) {
                bail_out(EXIT_FAILURE, "Closing stream failed.\n");
            }
        }
    } else {
        (void)expand(stdin, tabs);
    }

    exit(EXIT_SUCCESS);
}

static void bail_out(int exitcode, const char *fmt, ...)
{
    va_list ap;

    (void) fprintf(stderr, "%s: ", pgm_name);
    if (fmt != NULL) {
        va_start(ap, fmt);
        (void) vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    if (errno != 0) {
        (void) fprintf(stderr, ": %s", strerror(errno));
    }
    (void) fprintf(stderr, "\n");

    cleanup();
    exit(exitcode);
}


