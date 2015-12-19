/**
 *  @file client.c
 *  @author Constantin Schieber, e1228774
 *  @brief Client for playing the Mastermind game (even with algorithmus)
 *  @details Can solve the games in approximate 8 turns!!!!
 *  @date 07.11.2015
 * */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>

#include <sys/un.h>
#include <netdb.h>

/* === Constants === */

#define MAX_TRIES (35)
#define SLOTS (5)
#define COLORS (8)

#define READ_BYTES (2)
#define WRITE_BYTES (1)
#define BUFFER_BYTES (2)
#define SHIFT_WIDTH (3)
#define PARITY_ERR_BIT (6)
#define GAME_LOST_ERR_BIT (7)

#define EXIT_PARITY_ERROR (2)
#define EXIT_GAME_LOST (3)
#define EXIT_MULTIPLE_ERRORS (4)

#define LISTEN_BACKLOG (5)
#define MAXDATASIZE (100)



/* === Macros === */

#ifdef ENDEBUG
#define DEBUG(...) do { fprintf(stderr, __VA_ARGS__); } while(0)
#else
#define DEBUG(...)
#endif

/* Length of an array */
#define COUNT_OF(x) (sizeof(x)/sizeof(x[0]))

/* === Global Variables === */

/* Name of the program */
static const char *progname = "client"; /* default name */

/* File descriptor for server socket */
static int sockfd = -1;

/* File descriptor for connection socket */
static int connfd = -1;

/* This variable is set upon receipt of a signal */
volatile sig_atomic_t quit = 0;


/* === Type Definitions === */

struct opts {
    int portno;
    char* hostname;
};

/* For the list */
struct node {
    uint16_t val;
    struct node *next;
};

/* Global list for the comfort. */
struct node *root_all, *root_sel, *curr;

/* Global enum for comfort. */
enum { beige, darkblue, green, orange, red, black, violet, white };

/* === Prototypes === */

/**
 * @brief Removes bad answers from the guess list. 
 * @param s_answer Answer from server.
 * @param c_guess The previous guess from the client.
 * @return void
 */
static void remove_guesses(uint8_t s_answer, uint16_t c_guess);

/**
 * @brief Compute answer to request
 * @param req Client's guess
 * @param resp Buffer that will be sent to the client
 * @param secret The server's secret
 * @return Number of correct matches on success; -1 in case of a parity error
 */
static int compute_answer(uint16_t req, uint8_t *resp, uint8_t *secret);

/**
 * @brief Calculate the parity for the answer.
 * @param message The message which parity shall be calculated. 
 * @return void
 */
static void calculate_parity(uint16_t *message);

/**
 * @brief terminate program on program error
 * @param exitcode exit code
 * @param fmt format string
 */
static void bail_out(int exitcode, const char *fmt, ...);

/**
 * @brief Parse command line options
 * @param argc The argument counter
 * @param argv The argument vector
 * @param options Struct where parsed arguments are stored
 */
static void parse_args(int argc, char **argv, struct opts *options);

/**
 * @brief free allocated resources
 */
static void free_resources(void);

/* === Implementations === */

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
    //Clear list
    curr = root_all;

    while(curr)
    {
        struct node *next = curr->next;
        free(curr);
        curr = next;
    }

   /* clean up resources */
    if(connfd >= 0) {
        (void) close(connfd);
    }
    if(sockfd >= 0) {
        (void) close(sockfd);
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
    struct opts options;
    
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sockfd, s,i;
    
    /* Parse arguments. */
    parse_args(argc, argv, &options);
   
    /* Generate all possible outcomes. */
    for(i = 0; i < 32769; i++) {
        curr = (struct node*)malloc(sizeof(struct node*));
        curr->val = i;
        curr->next = root_all;
        root_all = curr;
    }

    /* Init getaddrinfo. */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

     if((s = getaddrinfo(options.hostname, argv[2], &hints, &result)) != 0) {
        bail_out(EXIT_FAILURE, "getaddrinfo: %s\n", gai_strerror(s));
    }
    
    /* Get a socket and connect try to connect to it. */
    for(rp = result; rp != NULL; rp = rp->ai_next) {
        if((sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
            continue;
        }

        if(connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break;  /* Success */
        }
        close(sockfd);
    }

    if(rp == NULL) {
        bail_out(EXIT_FAILURE, "Could not bind to socket\n");
    }

    freeaddrinfo(result);
    
    int round;
    uint16_t message = 18712;
    static uint8_t receive[1], send_b[1];
    uint8_t tmp_receive;
   
    for(round = 1; round < 35; round++) {
       /* Calculate answer for server. */
       (void)calculate_parity(&message); 

        /* Send answer to server. */
        send_b[0] = message & 0x00FF;
        send_b[1] = (message >> 8) & 0x00FF;
        if(send(sockfd, &send_b[0],2, 0) == -1) {
            bail_out(EXIT_FAILURE, "Error while sending.\n");
        }

       /* Wait for response from server. */
       if(recv(sockfd, &receive[0], 1, 0) == -1) {
            bail_out(EXIT_FAILURE, "Error while receiving bytes.\n");
       }

       /* Check wether we did something wrong. */
       switch(receive[0] >> 6) {
            case 1: 
                bail_out(PARITY_ERR_BIT, "Parity Error\n");
                break;
            case 2:
                bail_out(GAME_LOST_ERR_BIT, "Game Lost\n");
                break;
            case 3:
                bail_out(EXIT_MULTIPLE_ERRORS, "Parity Error, Game Lost\n");
            default:
                break;
       }

       if((receive[0] & 0x7) == 5) {
          break;
       }       

       /* Remove all guesses that can't reach the quaility of our answer. */
       tmp_receive = receive[0];
       (void)remove_guesses(tmp_receive, message);
       
       message = root_all->val;
    }
    exit(EXIT_SUCCESS);
}

static void remove_guesses(uint8_t s_answer, uint16_t c_guess) {
    
    uint16_t req = c_guess;
    uint8_t secret[SLOTS]; 
    struct node *curr_sel;
    int cnt = 0, j = 0;
    
    c_guess = c_guess & 0x7fff;
    s_answer = s_answer & 0x3f;
    curr = root_all;
    /* Extract our own answer... */
    for (j = 0; j < SLOTS; ++j) {
        int tmp = req & 0x7;
        secret[j] = tmp;
        req >>= SHIFT_WIDTH;
    }
    
    /* Create a list with matching answers */
    while(curr) {
        uint8_t tmp_result;
        /* Calculate local answer, cast on void because parity is already correct. */
        (void)compute_answer(curr->val, &tmp_result, secret); 
        if(tmp_result == s_answer) {
            curr_sel = (struct node*)malloc(sizeof(struct node*));
            curr_sel->val = curr->val;
            curr_sel->next = root_sel;
            root_sel = curr_sel;
            cnt++;
        }
        curr = curr->next;
    }
    /* Set the start of the list */
    curr = root_all;
    /* Cleanup */
    while(curr) {
        struct node *next = curr->next;
        free(curr);
        curr = next;
    }
    root_all = root_sel;
    root_sel = NULL;
}

/* Calculate colors for server. */
static void calculate_parity(uint16_t *message)
{
    int i, parity = 0;
    uint16_t mes = *message, tmp_mes = *message;    /* Dereference address for comfort. */
    
    /* Calculation of the parity bit. */ 
    for(i = 0; i < SLOTS; i++) {
       int tmp = tmp_mes & 0x07;
       parity ^= tmp ^ (tmp >> 1) ^ (tmp >> 2);
       tmp_mes >>= SHIFT_WIDTH; 
    }
    
    parity &= 0x1;
    parity = (parity << 15);
    mes =  mes + parity;

    
    *message = mes;
}

static void parse_args(int argc, char **argv, struct opts *options)
{
    char *port_arg;
    char *host_arg;
    char *endptr;
   

    if(argc > 0) {
        progname = argv[0];
    }
    if (argc != 3) {
        bail_out(EXIT_FAILURE,
            "Usage: %s <server-hostname> <server-port>", progname);
    }
    host_arg = argv[1];
    port_arg = argv[2];

    errno = 0;
    options->portno = strtol(port_arg, &endptr, 10);

    if ((errno == ERANGE &&
            (options->portno == LONG_MAX || options->portno == LONG_MIN))
        || (errno != 0 && options->portno == 0)) {
        bail_out(EXIT_FAILURE, "strtol");
    }

    if(endptr == port_arg) {
        bail_out(EXIT_FAILURE, "No digits were found");
    }

    /* If we got here, strtol() successfully parsed a number */

    if (options->portno < 1 || options->portno > 65535)
    {
        bail_out(EXIT_FAILURE, "Use a valid TCP/IP port range (1-65535)");
    }

    options->hostname = host_arg; 
}
 

static int compute_answer(uint16_t req, uint8_t *resp, uint8_t *secret)
{
    int colors_left[COLORS];
    int guess[COLORS];
    uint8_t parity_calc;
    int red, white;
    int j;

    /* extract the guess and calculate parity */
    parity_calc = 0;
    for (j = 0; j < SLOTS; ++j) {
        int tmp = req & 0x7;
        parity_calc ^= tmp ^ (tmp >> 1) ^ (tmp >> 2);
        guess[j] = tmp;
        req >>= SHIFT_WIDTH;
    }
    parity_calc &= 0x1;

    /* marking red and white */
    (void) memset(&colors_left[0], 0, sizeof(colors_left));
    red = white = 0;
    for (j = 0; j < SLOTS; ++j) {
        /* mark red */
        if (guess[j] == secret[j]) {
            red++;
        } else {
            colors_left[secret[j]]++;
        }
    }
    for (j = 0; j < SLOTS; ++j) {
        /* not marked red */
        if (guess[j] != secret[j]) {
            if (colors_left[guess[j]] > 0) {
                white++;
                colors_left[guess[j]]--;
            }
        }
    }

    /* build response buffer */
    resp[0] = red;
    resp[0] |= (white << SHIFT_WIDTH);
   return 0; 
   // if (parity_recv != parity_calc) {
   //     resp[0] |= (1 << PARITY_ERR_BIT);
   //     return -1;
   // } else {
   //     return red;
   // }
}
