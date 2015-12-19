/**
 *  @file stillepost.c
 *  @author Constantin Schieber, e1228774
 *  @brief Can play "Stille Post" 
 *  @details Via fork(2) and pipe(2) 
 *  @date 22.11.2015
 * */

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

/* === Global variables === */

/** \brief Verbosity on / off. */
static int verbosity = -1;	/* default off. */

/** \brief Length of <string>. */
static int length;

/** \brief Amount of children that should be created. */
static long child_cnt;

/** \brief <string> in main thread. */
static char *ext_buffer;

/** \brief <string> in child threads. */
static char *int_buffer;

/** \brief  Name of the program. */
static const char *progname = "stillepost";	/* default name */

/* === Prototypes === */

/**
 * @brief parses arguments and options 
 * @param argc argument count
 * @param argv argument vector
 */
static void parse_args(int argc, char **argv);

/**
 * @brief inserts a random char at a random position in the string 
 * @detailed random char from range [a-z][A-Z] at random position [0 to (length-1)]
 * @param buffer string that is operated on 
 */
static void swap_stuff(char *buffer);

/**
 * @brief Prints debug information when -v flag is set 
 * @param fmt format string 
 */
static void verbPrintf(const char *fmt, ...);

/**
 * @brief terminate program on program error 
 * @param exitcode exitcode
 * @param fmt format string
 */
static void bail_out(int exitcode, const char *fmt, ...);

/**
 * @brief free allocated resources
 */
static void free_resources(void);

/**
 * @brief one close to rule them all 
 * @param pipefd the fd array
 * @param pos pos of fd to close
 */
static void close_pipe(int *pipefd, int pos);

/**
 * @brief Program entry point
 * @param argc The argument counter
 * @param argv The argument vector
 * @return EXIT_SUCCESS on success
 * error, EXIT_FAILURE if anything went wrong
 */
int
main(int argc, char *argv[]) {
	/* File descriptor for unnamed pips. */
	int pipefd[4];
	int pipe_pos = 2, start_fd = 1;
	int i;
	int rw_size = 0;
	char mode;
	FILE *fp;
	pid_t cpid;

	memset(&pipefd[0], -1, sizeof(pipefd));
	/* Parse arguments. */
	parse_args(argc, argv);

	/* Allocate memory for input string. */
	ext_buffer = malloc((length) * sizeof(char));
    if (ext_buffer == NULL) {
        bail_out(EXIT_FAILURE, "malloc", progname);
    }
	for(i = 0; i < length; i++) {
		ext_buffer[i] = argv[optind][i];
	}

	/* Create all the children. */
	for(i = 0; i < child_cnt; i++) {
		if(i > 0) {
			close_pipe(pipefd, pipe_pos);
			close_pipe(pipefd, pipe_pos + 1);
		}

		/* Create Pipes for next child. */
		if(pipe(&pipefd[pipe_pos]) == -1) {
			bail_out(EXIT_FAILURE, "pipe", progname);
		}

		/* Last child created, first child to listen. */
		if(i == (child_cnt - 1)) {
			start_fd = pipe_pos + 1;
		}

		/* Fork! I am hungry. */
		cpid = fork();
		if(cpid == -1) {
			bail_out(EXIT_FAILURE, "fork", progname);
		}

		/* The Child Process gets here. */
		if(cpid == 0) {
			if(i > 0) {
				close_pipe(pipefd, pipe_pos + 1);
				close_pipe(pipefd, 2 - pipe_pos);
			}

			int_buffer = malloc((length) * sizeof(char));
            if (int_buffer == NULL) {
                bail_out(EXIT_FAILURE, "malloc", progname);
            }
			mode = 'r';
			fp = fdopen(pipefd[pipe_pos], &mode);
			if(fp == NULL) {
				bail_out(EXIT_FAILURE, "fdopen", progname);
			}

			rw_size = fread(int_buffer, sizeof(char), length, fp);
			if(rw_size != length) {
				(void)fclose(fp);
				bail_out(EXIT_FAILURE, "fread", progname);
			}
			(void)fclose(fp);

			verbPrintf("child%d : vorher : %s\n", i, int_buffer);
			swap_stuff(int_buffer);

			/* First child created, last to speak out loud. */
			if(i <= 0) {
				if(verbosity < 0) {
					(void)fprintf(stdout, "%s\n",
						      int_buffer);
				} else {
					verbPrintf("child%d : ende   : %s\n",
						   i, int_buffer);
				}
				free(int_buffer);
				_exit(EXIT_SUCCESS);
			}
            
            /* If it's not over yet. */
			verbPrintf("child%d : weiter : %s\n", i, int_buffer);
			mode = 'w';
			fp = fdopen(pipefd[3 - pipe_pos], &mode);
			if(fp == NULL) {
				bail_out(EXIT_FAILURE, "fdopen", progname);
			}

			rw_size =
				fwrite(int_buffer, sizeof(char), length, fp);

			if(rw_size != length) {
				(void)fclose(fp);
				bail_out(EXIT_FAILURE, "fwrite", progname);
			}

			(void)fclose(fp);
			
            free(int_buffer);
			_exit(EXIT_SUCCESS);
		}

		/* So that we don't override the same 2 pipes all the time. */
		pipe_pos = 2 - pipe_pos;
	}
	/* The parent process gets here. */
	close_pipe(pipefd, 0);
	close_pipe(pipefd, 2);
	if(start_fd == 1) {
		close_pipe(pipefd, 3);
	}
	if(start_fd == 3) {
		close_pipe(pipefd, 1);
	}

	verbPrintf("Parent : vorher : %s\n", ext_buffer);
	swap_stuff(ext_buffer);
	verbPrintf("Parent : weiter : %s\n", ext_buffer);
    
    /* Write to pipe. */
	mode = 'w';
	fp = fdopen(pipefd[start_fd], &mode);
	if(fp == 0) {
		bail_out(EXIT_FAILURE, "fdopen", progname);
	}
	if(fwrite(ext_buffer, sizeof(char), length, fp) != length) {
		bail_out(EXIT_FAILURE, "fwrite", progname);
	}

	(void)fclose(fp);

	close_pipe(pipefd, start_fd);

	/* Wait for children to terminate. */
	int tmp_cnt = 0;
	do {
		wait(NULL);
		tmp_cnt++;
	}
	while(tmp_cnt < child_cnt);

	free_resources();
	exit(EXIT_SUCCESS);
}

/* === Implementations === */

static void
bail_out(int exitcode, const char *fmt, ...) {
	va_list ap;

	(void)fprintf(stderr, "%s: ", progname);
	if(fmt != NULL) {
		va_start(ap, fmt);
		(void)vfprintf(stderr, fmt, ap);
		fflush(stderr);
		va_end(ap);
	}
	if(errno != 0) {
		(void)fprintf(stderr, ": %s", strerror(errno));
		fflush(stderr);
	}
	(void)fprintf(stderr, "\n");
	fflush(stderr);

	free_resources();
	exit(exitcode);
}

static void
swap_stuff(char *buffer) {
	/*All random. */
	srand((int)getpid());

	/* 57 --> [a-z][A-Z]. */
	int up_low = rand() % 2;
	char c;
	if(up_low == 1) {
		c = rand() % 26 + 'A';
	} else {
		c = rand() % 26 + 'a';
	}

	int pos = rand() % (length - 1);

	buffer[pos] = c;
}

static void
verbPrintf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);

	if(verbosity >= 0) {
		(void)vfprintf(stdout, fmt, args);
		(void)fflush(stdout);
	}
}

static void
free_resources(void) {
	(void)free(ext_buffer);
    (void)free(int_buffer);
}

static void
parse_args(int argc, char **argv) {
	int opt;
	char *endptr;

	progname = argv[0];

	if(argc < 3) {
		bail_out(EXIT_FAILURE, "Usage: %s <n> <string>", progname);
	}

	while((opt = getopt(argc, argv, "v")) != -1) {
		switch (opt) {
		case 'v':
			verbosity = 1;
			break;
		default:	/* '?' */
			bail_out(EXIT_FAILURE, "Usage: %s <n> <string>",
				 progname);
		}
	}

	/* Convert family size to long. */
	child_cnt = strtol(argv[optind], &endptr, 10);

	if((errno == ERANGE
	    && (child_cnt == LONG_MAX || child_cnt == LONG_MIN))
	   || (errno != 0 && child_cnt == 0)) {
		bail_out(EXIT_FAILURE, "strtol", progname);
	}

	if(argv[optind] == endptr) {
		bail_out(EXIT_FAILURE, "Usage: %s <n> <string>", progname);
	}

	/* n >= 2 it says, I can't handle more */
	if(child_cnt < 2 || child_cnt > 30500) {
		bail_out(EXIT_FAILURE, "Usage: %s <n> <string>\n 2<=n<=30500", progname);
	}

	/* Read length of string. */
	length = strlen(argv[++optind]);

	if(length <= 0) {
		bail_out(EXIT_FAILURE, "String shouldn't be empty", progname);
	}

	/* Amount of chars that fit on one line in verbose output.
	 * Alternative: RAND_MAX. */
	if(length >= 256) {
		bail_out(EXIT_FAILURE, "String shouldn't be empty", progname);
	}
}

static void
close_pipe(int *pipefd, int pos) {
	if(pipefd[pos] != -1) {
		(void)close(pipefd[pos]);
		pipefd[pos] = -1;
	}
}
