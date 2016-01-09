/**
 *  @file server_hangman.c
 *  @author Constantin Schieber, e1228774
 *  @brief server 
 *  @details
 *  @date 
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

/* === Enumerations === */

/* === Structures === */

struct GameState {
   char *secret_word;
};

struct WordList {
    char *word;
    struct WordList *next;
};

struct ClientList {
    int server_id;          /* ID of client on server. */
    char secret_word[MAX_DATA];      /* Current secret word. */
    char client_word[MAX_DATA];
    char guess;
    uint8_t errors;         /* Amount of errors made. */
    uint8_t game_count;     /* Amount of games played. */
    uint8_t status_id;
    struct ClientList *next; 
};

/* === Global Variables === */

/** @brief Name of the program. */
static const char *progname = "server"; /* default name */

/** @brief Contains the dictionary */
static char **strings = NULL;

/** @brief Helps with containing the dicionary */
static char *string;

/** @brief Helps with containing the dicionary */
static uint8_t dict_size;

/** @biref Filedescriptor for the Input Stream */
static FILE *in_stream;

/** @brief */
static struct Myshm *shared;  

/** @brief */
static struct ClientList *client_list = NULL;  

/** @brief */
static sem_t *s_server;  

/** @brief */
static sem_t *s_client;  

/** @brief */
static sem_t *s_return;  

/** @brief */
static char sem_set = -1;  

/** @brief Indicates wether SIGTERM OR SIGINT got set. */
volatile sig_atomic_t want_quit = 0;

/* === Prototypes === */

/**
 * @brief Catches SIGINT & SIGTERM for gracefull termination 
 * @param 
 * @return 
**/
static void signal_handler(int signal);

/**
 * @brief Terminates the program gracefully
 * @param 
 * @return 
**/
static void bail_out(int exitcode, const char *fmt, ...);

/**
 * @brief Reads the dictionary
 * @param 
 * @return 
**/
static void read_dict();

/**
 * @brief Frees resources on exit 
 * @return void
**/
static void free_resources();

/**
 * @brief sets up a new game 
 * @param 
 * @return 
**/
static void create_game(struct ClientList *el_cur); 
/**
 * @brief checks the guess 
 * @param 
 * @return 
**/
static void check_guess(struct ClientList *el_cur);

/* === Implementations === */

static void signal_handler(int signal) {
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

static void free_resources(void) 
{
    if (client_list != NULL) {
        struct ClientList *el_cur;
        struct ClientList *el_next;
        el_cur = client_list;

        while(el_cur->next != NULL) {
            el_next = el_cur->next;    
            free(el_cur);
            el_cur = el_next;
            if (sem_post(s_client) == -1) {
                (void) fprintf(stderr, 
                                "sem_post(3) failed: %s", strerror(errno));
            }
        }
    }

    if (shared != NULL) {
        shared->sc_terminate = 1;
        DEBUG("MUNMAP\n");
        if (munmap(shared, sizeof *shared) == -1) {
            (void) fprintf(stderr, 
                    "%s: munmap failed: %s\n", progname, strerror(errno));
        } 

        DEBUG("SHM_UNLINK\n");
        if (shm_unlink(SHM_NAME) == -1) {
            (void) fprintf(stderr, 
                    "%s: shm_unlink failed: %s\n", progname, strerror(errno));
        } 
    }

    
    DEBUG("SEM_UNLINK\n");
    /* Close semaphores */
    if (sem_set >= 0) {

    if (sem_close(s_server) == -1) {
            (void) fprintf(stderr, 
                "%s: sem_close failed: %s\n", progname, strerror(errno));
    }
    
    if (sem_unlink(S_SERVER) == -1) {
            (void) fprintf(stderr, 
                "%s: sem_unlink failed: %s\n", progname, strerror(errno));
    }
  
    if (sem_close(s_client) == -1) {
            (void) fprintf(stderr, 
                "%s: sem_close failed: %s\n", progname, strerror(errno));
    }
    
    if (sem_unlink(S_CLIENT) == -1) {
            (void) fprintf(stderr, 
                "%s: sem_unlink failed: %s\n", progname, strerror(errno));
    }
   
    if (sem_close(s_return) == -1) {
            (void) fprintf(stderr, 
                "%s: sem_close failed: %s\n", progname, strerror(errno));
    }
    
    if (sem_unlink(S_RETURN) == -1) {
            (void) fprintf(stderr, 
                "%s: sem_unlink failed: %s\n", progname, strerror(errno));
    }
    sem_set = -1;
    }
    
    if (strings != NULL) {
        free(strings);
    }

}

static char *get_strings() 
{
    char *string = NULL;
    char ch;
    size_t len = 0;

    while (EOF != (ch = fgetc(in_stream)) && ch != '\n') {
        string  = (char*) realloc(string, len+2);
        string[len++] = toupper(ch);

        if (len >= MAX_DATA) {
            bail_out(EXIT_FAILURE, "Input too long\n");
        }
    }
    if(string)
        string[len] = '\0';

    return string;
}

static void read_dict()
{
    int index;
    for (index = 0; (string = get_strings()); ++index) {
        strings = (char**) realloc(strings, (index+1)*sizeof(*strings));
        strings[index] = string;
    }
    dict_size = index;
}

static void create_game(struct ClientList *el_cur) 
{
    if (el_cur->game_count >= dict_size) {
        /* No new game possible. */
        el_cur->status_id = EndGame;
    } else {
        /* Assign secret word. */
        DEBUG("secret word before: %s\n", strings[el_cur->game_count]);
        strncpy(el_cur->secret_word, strings[el_cur->game_count], MAX_DATA);
        el_cur->errors = 0;
        el_cur->status_id = Running; 
        el_cur->game_count = el_cur->game_count + 1; 
        if (memset(el_cur->client_word, '_', MAX_DATA) != el_cur->client_word) {
            bail_out(EXIT_FAILURE, "memset(3) failed\n");
        }
         
        int i = 0;
        while(el_cur->secret_word[i] != '\0') {
            if (el_cur->secret_word[i] == ' ') {
                el_cur->client_word[i] = ' ';
            }
            i++;
        }
        el_cur->client_word[i] = '\0';
        
        DEBUG("Spiel erstellt: %s Secret Word: %s\n", 
                shared->s_word, el_cur->secret_word);
    }
}

static void check_guess(struct ClientList *el_cur) 
{
    int i = 0;
    char guess = el_cur->guess; 
    int error = 1;
    
    /* Check if the guess was correct. */ 
    while (el_cur->secret_word[i] != '\0') {
        if (el_cur->secret_word[i] == guess) {
            error = 0;
            el_cur->client_word[i] = guess;
        }
        i++;
    }

    el_cur->errors = el_cur->errors + error;

    /* Check correctness of word. */
    i = 0;
    while (el_cur->client_word[i] != '\0' && el_cur->client_word[i] != '_') {
        i++;
    }

    DEBUG("SERVER: WORD LENGHT %d\n", i);
    
    /* If all '_' are replaced, the game is won. */
    if (i == strlen(el_cur->secret_word)) {
        el_cur->status_id = Won;
    }

    /* Check if number of errors exceeds limit. */ 
    if (el_cur->errors >= MAX_ERRORS) {
        el_cur->status_id = Lost;
    }
    DEBUG("Came through the guessing part...\n");
}

/**
 * @brief Program entry point.
 * @param argc The argument count
 * @param argv The argument vector
 * @return EXIT_SUCCESS, EXIT_PARITY_ERROR, EXIT_MULTIPLE_ERRORS
**/
int main(int argc, char *argv[])  
{
    uint8_t client_count = 0;
    int count;
    int shmfd;
    
    //atexit(free_resources);
    
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
    
    /* Handle arguments */
    if (argc > 0) {
       progname = argv[0]; 
    }
    
    if (argc > 2) {
        fprintf(stderr, "USAGE: %s [input_file]", progname);
        exit(EXIT_FAILURE);
    }

    switch (argc) {
    /* Handle dict input from stdin */
    case 1: 
        in_stream = stdin;
        read_dict();
    break;
    /* Handle dict input from file */
    case 2: 
		if ((in_stream = fopen(argv[1], "r")) == NULL)
		{
            bail_out(EXIT_FAILURE, "Invalid File");
		}
        read_dict();
    break;
    default:
        fprintf(stderr, "USAGE: %s [input_file]", progname);
        exit(EXIT_FAILURE);
        break; 
    }

    for(count = 0; count < 3; ++count) {
        printf("Text: %s\n", strings[count]);
    }

    /* Create a new Shared Memory Segment (shm_open) */
    shmfd = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL, PERMISSION);

    if (shmfd == (-1)) {
        bail_out(EXIT_FAILURE, "shm_open failed");
    }
    
    /*  Truncate a file to a specified length 
        extend (set size) */ 
    if (ftruncate(shmfd, sizeof *shared) == -1) {
        (void) close(shmfd);
        bail_out(EXIT_FAILURE, "ftruncate failed");
    }

    /* Map shared memory object */
    shared = mmap(NULL, sizeof *shared,
                    PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);

    if (shared == MAP_FAILED) {
        (void) close(shmfd);
        bail_out(EXIT_FAILURE, "MMAP failed");
    }

    if (close(shmfd) == -1) {
        bail_out(EXIT_FAILURE, "close(shmfd) failed");
    }
    
    shared->sc_terminate = -1;

    /* Create new named semaphores */
    s_server = sem_open(S_SERVER, O_CREAT | O_EXCL, PERMISSION, 0);
    s_client = sem_open(S_CLIENT, O_CREAT | O_EXCL, PERMISSION, 1);
    s_return = sem_open(S_RETURN, O_CREAT | O_EXCL, PERMISSION, 0);
    
    if(s_server == SEM_FAILED || 
        s_client == SEM_FAILED || 
        s_return == SEM_FAILED) {
        bail_out(EXIT_FAILURE, "sem_open(3) failed"); 
    }

    sem_set = 1;

    struct ClientList *el_pre = NULL;
    struct ClientList *el_cur = NULL;
    
    /* Keep server open until it gets killed. */
    while (!want_quit) {
        /* Critical section entry. */
        /* Wait until server is allowed to access SHM. */
        if (sem_wait(s_server) == -1) {
            if(errno == EINTR) continue;
            bail_out(EXIT_FAILURE, "sem_wait(3) failed");
        }
        
        /* Setup data for existing client. */
        if (shared->s_id >= 0) {
            el_cur = client_list;
            while (el_cur != NULL && el_cur->server_id != shared->s_id) {
                printf("elpre\n");
                el_pre = el_cur;
                printf("elcur\n");
                el_cur = el_cur->next;
            }

            if (el_cur == NULL) {
                bail_out(EXIT_FAILURE, "Client does not exist");
            }

            el_cur->guess = shared->c_guess;
        }
        
        /* Setup new client to list. */
        if (shared->s_id == -1) {
            /* Allocate element and set to zero. */
            el_cur = 
                 (struct ClientList *) calloc (1, sizeof(struct ClientList)); 
            if (el_cur == NULL) {
                bail_out(EXIT_FAILURE, "calloc(3) failed");
            }

            /* Assign unique ID based on client count. */
            el_cur->server_id = client_count++;
            el_cur->game_count = 0;
            
            /* Add the current element to our list. */
            el_cur->next = client_list;
            client_list = el_cur;
        }
        
        /* Check if client has terminated. */
        if (shared->sc_terminate >= 1) {
            DEBUG("Terminating client...\n");
            /* Remove client from list. */
            if (client_list == el_cur) {
                client_list = el_cur->next;
            } else {
                el_pre->next = el_cur->next;
            }

            free(el_cur);
            /* Free allocated resources. */
            /* Reset sc_terminate. */
            shared->sc_terminate = -1;
            if (sem_post(s_client) == -1) {
                bail_out(EXIT_FAILURE, "sem_post(3) failed");
            }
            /* Skip the rest of the game as we have nothing to do here */
            continue;
        }

        /* Check game status of client. */
        if (shared->status_id == CreateGame) {
            /* Start a new game. */
            DEBUG("Setting up game for client %d\n", shared->s_id);
            create_game(el_cur);
            
        }

        if (shared->status_id == Running) {
            /* Check guess. */
            DEBUG("Checking guess of client %d\n", shared->s_id);
            check_guess(el_cur);
        }
        
        /* Write server answer back into SHM. */
        shared->s_id = el_cur->server_id;
        shared->status_id = el_cur->status_id;
        shared->s_errors = el_cur->errors;
        strncpy(shared->s_word, el_cur->client_word, MAX_DATA);
        
        /* Let the client know that there is an answer. */
		if (sem_post(s_return) == -1) { 
            bail_out(EXIT_FAILURE, "sem_post(3) failed");
        }

        /* critical section end. */

    }
    /* Free stuff */
    free(strings);
}
