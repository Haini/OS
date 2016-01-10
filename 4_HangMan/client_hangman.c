/**
 *  @file client_hangman.c
 *  @author Constantin Schieber, e1228774
 *  @brief client
 *  @details GameLogic for Hangman
 *  @date 12.01.2016
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <ctype.h>
#include "common_hangman.h"

/* === Macros === */
#ifdef ENDEBUG
#define DEBUG(...) do { fprintf(stderr, __VA_ARGS__); } while(0)
#else
#define DEBUG(...)
#endif

/* === Structures === */

/* === Global Variables === */

/** @brief Name of the program. */
static const char *progname = "client"; /* default name */

/** @brief Contains the game state of ONE client at a time. */
static struct Myshm *shared;  

/** @brief Server can access SHM, controlled exclusive by clients. */
static sem_t *s_server;  

/** @brief ONE client can access SHM, controlled by server / clients. */
static sem_t *s_client;  

/** @brief Server tells a waiting client that it can access it's answer now. */ 
static sem_t *s_return;  

/** @brief Unique ID, gets set by server. */
static int client_id = -1;  

/** @brief How many games were won. */
static uint8_t win_cnt = 0; 

/** @brief How many games were lost. */
static uint8_t loss_cnt = 0; 

/** @brief Regular, well defined termination. */
static char regular_exit = -1; 

/** @brief Indicates wether SIGTERM OR SIGINT got set. */
volatile sig_atomic_t want_quit = 0;

/* === Prototypes === */

/**
 * @brief Catches SIGINT & SIGTERM for gracefull termination 
 * @param signal Signal from Sigaction
**/
static void signal_handler(int signal);

/**
 * @brief Terminates the program gracefully
 * @param exitcode The Exitcode 
 * @param fmt format string
**/
static void bail_out(int exitcode, const char *fmt, ...);

/**
 * @brief Frees resources on exit 
 * @return void
**/
static void free_resources();

/**
 * @brief Hangs poor Mr. ASCII, based on errors. 
 * @param errors Errors that were made during the guessing
 * @return void 
**/
static void hang_him(uint8_t errors);


/* === Implementations === */

static void signal_handler(int signal) {
    regular_exit = 1;
    free_resources();
    exit(EXIT_SUCCESS);
}

static void bail_out(int exitcode, const char *fmt, ...)
{
    va_list ap;

    (void) fprintf(stderr, "%s: ", progname);
    if (fmt != NULL) {
        va_start(ap, fmt);
        (void) vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    if (errno != 0) {
        (void) fprintf(stderr, ": %s", strerror(errno));
    }
    (void) fprintf(stderr, "\n");

    free_resources();
    exit(exitcode);
}

static void free_resources() 
{
    if (regular_exit >= 1) {
        if (sem_wait(s_client) == -1) {
            if (errno != EINTR) {
                (void) fprintf(stderr, "%s: sem_wait\n", progname);
            }
            else {
                (void) fprintf(stderr, "%s: all is lost\n", progname);
            }
        } else {
            DEBUG("Sending termination info\n");	
            if (shared != NULL) {
                shared->sc_terminate = 1;
                shared->s_id = client_id;
            }
        }
        
        if (sem_post(s_server) == -1) {
            (void) fprintf(stderr, "%s: sem_post\n", progname);
        }
    }

    DEBUG("CLIENT: Passed terminate\n");

    /* Free stuff here */
	if (shared != NULL) {
		if (munmap(shared, sizeof *shared) == -1) {
			(void) fprintf(stderr, "%s: munmap: %s\n", progname, strerror(errno));
		}
	}
    
    if (s_server != NULL && sem_close(s_server) == -1) {
        (void) fprintf(stderr, "%s: sem_close on %s: %s\n", progname,S_SERVER , strerror(errno));
    }

	if (s_client != NULL && sem_close(s_client) == -1) {
        (void) fprintf(stderr, "%s: sem_close on %s: %s\n", progname, S_CLIENT, strerror(errno));
    }

	if (s_return != NULL && sem_close(s_return) == -1) {
        (void) fprintf(stderr, "%s: sem_close on %s: %s\n", progname, S_RETURN, strerror(errno));
    }
}

static void hang_him(uint8_t errors) {
    
    switch (errors) {
        case 0:
            printf("\n");
            printf("|_____|\n");
            break;
        case 1:
            printf("\n");
            printf("|__ __|\n");
            printf("|_____|\n");
            break;
        case 2:
            printf("\n");
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("|__|__|\n");
            printf("|_____|\n");
            break;
        case 3:
            printf("\n");
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("|__|__|\n");
            printf("|_____|\n");
            break;
        case 4:
            printf("\n");
            printf("   _______\n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("|__|__|\n");
            printf("|_____|\n");
            break;
        case 5:
            printf("\n");
            printf("   _______\n"); 
            printf("   |/  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("|__|__|\n");
            printf("|_____|\n");
            break;
        case 6:
            printf("\n");
            printf("   _______\n"); 
            printf("   |/    |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("|__|__|\n");
            printf("|_____|\n");
            break;
        case 7:
            printf("\n");
            printf("   _______\n"); 
            printf("   |/    |  \n"); 
            printf("   |    (_)\n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("|__|__|\n");
            printf("|_____|\n");
            break;
        case 8:
            printf("\n");
            printf("   _______\n"); 
            printf("   |/    |  \n"); 
            printf("   |    (_)\n"); 
            printf("   |    \\|/\n"); 
            printf("   |     |\n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("|__|__|\n");
            printf("|_____|\n");
            break;
        case 9:
            printf("\n");
            printf("   _______\n"); 
            printf("   |/    |  \n"); 
            printf("   |    (_)\n"); 
            printf("   |    \\|/\n"); 
            printf("   |     |\n"); 
            printf("   |    / \\\n"); 
            printf("   |  \n"); 
            printf("   |  \n"); 
            printf("|__|__|\n");
            printf("|_____|\n");
            break;
        default:
            break;

    }
}

/**
 * @brief Program entry point.
 * @param argc The argument count
 * @param argv The argument vector
 * @return EXIT_SUCCESS, EXIT_PARITY_ERROR, EXIT_MULTIPLE_ERRORS
**/
int main(int argc, char *argv[])  
{
    int shmfd; 
    uint8_t errors = 0;                 /* Errors made by user. */
    char guess = 0;                     /* Guess of the user. */
    char server_answer[MAX_DATA];       /* Contains answer from server. */
    char chars_guessed[ALPHABET_CNT];   /* Contains already guessed chars. */
    enum StatusID status_id = CreateGame; /* Represents current game status. */
   
    /* Handle input. */
    if (argc > 0) {
        progname = argv[0];
    }

    if (argc >= 2) {
        fprintf(stderr, "USAGE: %s", progname);
        exit(EXIT_FAILURE);
    }

    if (memset(chars_guessed, '0', ALPHABET_CNT) != chars_guessed) {
        bail_out(EXIT_FAILURE, "memset(3) failed");
    }
    
    if (memset(server_answer, '\0', MAX_DATA) != server_answer) {
        bail_out(EXIT_FAILURE, "memset(3) failed");
    }

    /* setup signal handlers */
    const int signals[] = {SIGINT, SIGTERM};
    struct sigaction s;

    s.sa_handler = signal_handler;
    s.sa_flags   = 0;
    if(sigfillset(&s.sa_mask) < 0) {
        bail_out(EXIT_FAILURE, "sigfillset");
    }
    for(int i = 0; i < COUNT_OF(signals); i++) {
        if (sigaction(signals[i], &s, NULL) < 0) {
            bail_out(EXIT_FAILURE, "sigaction");
        }
    }

    /* Open a new Shared Memory Object (shm_open) */
    shmfd = shm_open(SHM_NAME, O_RDWR, PERMISSION);

    if (shmfd == (-1)) {
        bail_out(EXIT_FAILURE, "shm_open(3) failed");
    }

    /* Truncate. */
    if (ftruncate(shmfd, sizeof *shared) == -1) {
        (void) close(shmfd);
        bail_out(EXIT_FAILURE, "ftruncate(3) failed");
    }

    /* Map shared memory object. */
    shared = mmap(NULL, sizeof *shared,
                    PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);

    if (shared == MAP_FAILED) {
        (void) close(shmfd);
        bail_out(EXIT_FAILURE, "mmap(3) failed");
    }

    if(close(shmfd) == -1) {
        bail_out(EXIT_FAILURE, "close(shmfd) failed");
    }

    /* Create new named semaphores. */
    s_server = sem_open(S_SERVER, 0); 
    s_client = sem_open(S_CLIENT, 0); 
    s_return = sem_open(S_RETURN, 0); 

    if(s_server == SEM_FAILED || 
        s_client == SEM_FAILED || 
        s_return == SEM_FAILED) {
        bail_out(EXIT_FAILURE, "sem_open(3) failed"); 
    }
    
    /* Start of game. */
    while (!want_quit) {
        
        /* Game already accepted by server. */
        if (status_id == Running) {
            char tmp;
            printf("Please enter your guess [a-z]: ");
            guess = fgetc(stdin);  
            if (guess == '\n') {
                continue;
            }
            
            while((tmp = getchar()) != '\n' && tmp != EOF);

            if(!isalpha(guess)) {
                fprintf(stderr, "Enter a valid letter [a-z]!\n");
                continue;
            }

            guess = toupper(guess);
            
            /* Check if character was already used. */
            int i = 0;
            printf("Already used chars: |");
            while (chars_guessed[i] != guess && i < ALPHABET_CNT) {
                if (chars_guessed[i] == '0') {
                    chars_guessed[i] = guess;
                    printf(" %c |", chars_guessed[i]);
                    i = ALPHABET_CNT;
                    break;
                }
                printf(" %c |", chars_guessed[i]);
                i++;
            }
            printf("\n"); 
            if (i < ALPHABET_CNT) {
                fprintf(stderr, "Enter a not yet entered letter\n");
                continue;
            }

            DEBUG("GUESS %c\n", guess);

        }

        /* Write guess to SHM. */
        if (sem_wait(s_client) == -1) {
            if (errno == EINTR) {
                continue;
            }
            bail_out(EXIT_FAILURE, "sem_wait(3) failed");
        }
        
        DEBUG("checking terminate: %d\n", shared->sc_terminate);
        if (shared->sc_terminate >= 1) {
            bail_out(EXIT_FAILURE, "Server terminated");
        }

        shared->status_id = status_id;
        shared->s_id = client_id;
        shared->c_guess = guess;
        
        /* Enable server to read from SHM. */
        if (sem_post(s_server) == -1) {
            bail_out(EXIT_FAILURE, "sem_post(3) failed");
        }


        /* Read answer from SHM. */
        if (sem_wait(s_return) == -1) {
            if (errno == EINTR) {
                continue;
            }
            bail_out(EXIT_FAILURE, "sem_wait(3) failed");
        }

        client_id = shared->s_id;
        status_id = shared->status_id;
        errors = shared->s_errors;
        strncpy(server_answer, shared->s_word, MAX_DATA);  
        /* Output output. */
        hang_him(errors);
        printf("Secret Word: %s\n", server_answer);
        printf("Error Counter: %d\n", errors);
        
        /* Let other clients access SHM. */
        if (sem_post(s_client) == -1) {
            bail_out(EXIT_FAILURE, "sem_post(3) failed");
        }
        
        /* Handle the rest of our task */
        if (status_id == Won) {
           printf("You won, wanna play another? Y/N\n");      
           win_cnt++;
        }

        if (status_id == Lost) {
           printf("You lost, wanna play another? Y/N\n");      
           loss_cnt++;
        }

        if (status_id == Won || status_id == Lost) {
            DEBUG("wating for input...\n");
            int c;
            
            c = getchar(); 
            c = toupper(c);

            if (c == 'Y') {
                status_id = CreateGame;  
                if (memset(chars_guessed, '0', MAX_ERRORS) != chars_guessed) {
                    bail_out(EXIT_FAILURE, "memset(3) failed");
                }
            } else {
                break;
            }
            while ((c = getchar()) != '\n' && c != EOF);
        }

        if (status_id == EndGame) {
            printf("\nThere are no more words that you could conquer, so here\t"
                       " have these stats instead: \n\t"
                       " Won Games:  %d\n\t"
                       " Lost Games: %d\n\t"
                       " Rounds Played: %d\n", 
                        win_cnt, loss_cnt, (win_cnt+loss_cnt));
            regular_exit = 1;
            bail_out(EXIT_SUCCESS, "See you later...\n");
        }
        
        
    }
    printf("\nYou have chosen to stop the riddles, so here\t"
               " have these stats instead: \n\t"
               " Won Games:  %d\n\t"
               " Lost Games: %d\n\t"
               " Rounds Played: %d\n", 
                win_cnt, loss_cnt, (win_cnt+loss_cnt));
    regular_exit = 1;
    printf("Exiting...\n");
    free_resources();
}
