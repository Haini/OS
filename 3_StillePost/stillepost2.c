#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

/* Global variables. */
int verbosity = -1;
int length = 1;

static void
swap_stuff(char *buffer)
{
    /*All random.*/
    srand((int)getpid());            

    /* 57 --> [a-z][A-Z]. */
    int up_low = rand() % 2;
    char c;
    if (up_low == 1) {
        c = rand() % 26 + 'A';
    } else {
        c = rand() % 26 + 'a';
    }

    int pos= rand() % (length - 1);

    buffer[pos] = c;
}   

static void
verbPrintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    if (verbosity >= 0) {
        vfprintf (stdout, format, args);
        fflush(stdout);
    }
}

int
main(int argc, char *argv[])
{
    int pipefd[4];
    int start_fd = 1;
    int child_cnt;
    int i, pipe_pos = 2;
    int opt;
    char buf;
    char *ext_buffer;
    pid_t cpid;


    if (argc < 3) {
        fprintf(stderr, "Usage: %s <n> <string>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    while ((opt = getopt(argc, argv, "v")) != -1) {
        switch (opt) {
            case 'v':
                verbosity = 1;
                break;
            default: /* '?' */
                fprintf(stderr, "Usage: %s <n> <string> [-v]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    child_cnt = atoi(argv[optind++]);
    if(child_cnt < 2) {
        fprintf(stderr, "Usage: %s <n> <string> [-v]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    length = strlen(argv[optind]);

    ext_buffer = malloc((length) * sizeof(char));
    int j;

    for(j = 0; j < length; j++) {
        ext_buffer[j] = argv[optind][j];
    }
    verbPrintf("Parent : vorher : %s\n", ext_buffer);
    swap_stuff(ext_buffer);
    verbPrintf("Parent : weiter : %s\n", ext_buffer);

    /* Create all the children. */
    for (i = 0; i < child_cnt; i++) {
        close(pipefd[pipe_pos]);
        close(pipefd[pipe_pos+1]);
        /* Create Pipes for NextChild */
        if (pipe(&pipefd[pipe_pos]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        if(i == (child_cnt-1)) { start_fd = pipe_pos + 1; }

        /* Fork! I am hungry. */
        cpid = fork();
        if (cpid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        /* The Child Process gets here. */
        if (cpid == 0) {

            close(pipefd[pipe_pos + 1]);
            close(pipefd[2 - pipe_pos]);

            char *buffer = malloc((length) * sizeof(char));
            int j = 0; 

            if(i == 0) {
                while (read(pipefd[pipe_pos], &buf, 1) > 0) {
                    buffer[j] = buf;
                    j++;
                }

                verbPrintf("child%d : vorher : %s\n", i, buffer);
                swap_stuff(buffer);
                verbPrintf("child%d : weiter : %s\n", i, buffer);

                if(verbosity < 0) {
                    write(STDOUT_FILENO, buffer, length);
                    write(STDOUT_FILENO, "\n", 1);
                }
                free(buffer);     
                close(pipefd[pipe_pos]);
                _exit(EXIT_SUCCESS);
            }

            while (read(pipefd[pipe_pos], &buf, 1) > 0) {
                buffer[j] = buf;
                j++;
            }

            verbPrintf("child%d : vorher : %s\n", i, buffer);
            swap_stuff(buffer);
            verbPrintf("child%d : weiter : %s\n", i, buffer);

            write(pipefd[3 - pipe_pos], buffer, length);

            free(buffer);     
            close(pipefd[pipe_pos+1]);
            close(pipefd[3 - pipe_pos]);
            _exit(EXIT_SUCCESS);
        }

        /* So that we don't override the same 2 pipes all the time. */
        pipe_pos = 2 - pipe_pos;
    }
    close(pipefd[0]);
    close(pipefd[2]);
    if(start_fd == 1) close(pipefd[3]);
    if(start_fd == 3) close(pipefd[1]);


    write(pipefd[start_fd], ext_buffer, length);

    close(pipefd[start_fd]);

    int tmp_cnt = 0; 
    do {
        wait(NULL);
        tmp_cnt++;
    } while (tmp_cnt < child_cnt) ;

    free(ext_buffer);
    exit(EXIT_SUCCESS);
}
