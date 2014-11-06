/*
 *
 *
 *
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
#include <arpa/inet.h>
#include <sys/wait.h>
#include <netdb.h>

/* === Constants === */

#define MAX_TRIES (35)
#define SLOTS (5)
#define COLORS (8)

#define READ_BYTES (1)
#define WRITE_BYTES (2)
#define BUFFER_BYTES (1)
#define SHIFT_WIDTH (3)
#define PARITY_ERR_BIT (6)
#define GAME_LOST_ERR_BIT (7)

#define EXIT_PARITY_ERROR (2)
#define EXIT_GAME_LOST (3)
#define EXIT_MULTIPLE_ERRORS (4)

/* === Macros === */

#ifdef ENDEBUG
#define DEBUG(...) do { fprintf(stderr, __VA_ARGS__); } while(0)
#else
#define DEBUG(...)
#endif

/* Length of an array */
#define COUNT_OF(x) (sizeof(x)/sizeof(x[0]))

/* === Gloabl Variables === */

/* Name of the program */
static const char *progname = "client"; /* default name */

/* File descriptor for server socket */
static int sockfd = -1;

/* File descriptor for connection socket */
static int connfd = -1;

/* This variable is set to ensure cleanup is performed only once */
volatile sig_atomic_t terminating = 0;

/* ==== Constructors === */

struct node {
	uint16_t val;
	struct node *next;
};

struct node *root_all, *root_sel, *curr;

/* === Prototypes === */

/*
 * @brief Calculate Parity Bit
 *
 * @param buffer Buffer whose parity has to be calculated
 * @return Returns the parity bit 
 */
static uint16_t calc_parity(uint16_t buffer);

/*
 * @brief Write message to socket
 * 
 * @param sockfd_con Socket to write to
 * @param buffer Buffer with data to write
 * @param n Size to write
 * @return Pointer to buffer on success, else NULL
 */
static uint16_t *write_to_server(int sockfd_con, uint16_t *buffer, size_t n);

/*
 * @brief Read message from socket
 *
 * @param sockfd_con Socket to read from
 * @param buffer Buffer where read data is stored
 * @param n Size to read
 * @return Pointer to buffer on success, else NULL
 */
static uint8_t *read_from_server(int sockfd_con, uint8_t *buffer, size_t n);

/*
 * @brief Compute local answer
 * @param req My guess
 * @param secret My own secret
 * @return Number of correct matches
 */
static int compute_answer(uint16_t req, uint8_t *resp, uint8_t *secret);

/**
 * @brief Remove inconsistent guesses
 * @param answer Contains the answer by the server
 * @param guess Contains the old guess
 */
static void remove_guesses(uint8_t *answer, uint16_t *guess);

/**
 * @brief Get the next guess
 * @param guess Reference to an uint16_t where I store the answer
 */
static void get_next_guess(uint16_t *guess);

/**
 * @brief terminate program on program error
 * @param eval exit code
 * @param fmt format string
 */
static void bail_out(int eval, const char *fmt, ...);

/**
 * @brief Signal Handler
 * @param sis Signal number catched
 */
static void signal_handler(int sig);

/**
 * @brief free allocated resources
 */
static void free_resources(void);


/* === Implementations === */

static uint16_t calc_parity(uint16_t buffer)
{
	int i = 0;
	uint16_t parity_calc = 0;

	for(i = 0; i < SLOTS; ++i)
	{
		int tmp = buffer & 0x07;
		parity_calc ^= tmp ^ (tmp >> 1) ^ (tmp >> 2);
		buffer >>= SHIFT_WIDTH;
	}

	parity_calc &= 0x1;

	parity_calc = parity_calc << 15;

	return parity_calc; 
}

static uint16_t *write_to_server(int sockfd_con, uint16_t *buffer, size_t n)
{
	
	uint16_t parity_bit = calc_parity(*buffer);
	
	uint16_t answer = ((*buffer) | parity_bit);
	
	size_t r = send(sockfd_con, &answer, n, 0);

	if(r == -1) {
		bail_out(EXIT_FAILURE, "Error while writing to server\n");
	}

	return buffer;
}

static uint8_t *read_from_server(int sockfd_con, uint8_t *buffer, size_t n)
{
	size_t r = recv(sockfd, &buffer[0], n, 0);

	if(r == -1) {
		bail_out(EXIT_FAILURE, "Error while reading from server\n");
	}

	return buffer;
}

static int compute_answer(uint16_t req, uint8_t *resp, uint8_t *secret)
{
	int colors_left[COLORS];
	int guess[COLORS];
	int red, white;
	int j;


	/* extract the guess */
	for (j = 0; j < SLOTS; ++j) {
		int tmp = req & 0x7;
		guess[j] = tmp;
		req >>= SHIFT_WIDTH;
	}

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

	return red;
}

static void remove_guesses(uint8_t *answer, uint16_t *guess)
{
	uint8_t secret[SLOTS];
	uint8_t result[1];

	for(int i = 4; i >= 0; i--)
	{
		secret[i] = (((*guess) & (7 << (3*i))) >> (3*i));
	}

	curr = root_all;
	struct node *curr_sel;

	while(curr)
	{
		compute_answer(curr->val, &result[0], secret);

		if(result[0] == *answer)
		{
			curr_sel = (struct node*)malloc(sizeof(struct node*));
			curr_sel->val = curr->val;
			curr_sel->next = root_sel;
			root_sel = curr_sel;
		}

		curr=curr->next;
	}

	/*remove all the things*/

	curr = root_all;
	while(curr)
	{
		struct node *next = curr->next;
		free(curr);
		curr = next;
	}

	root_all = root_sel;
	root_sel = NULL;
}

static void get_next_guess(uint16_t *guess)
{
	uint16_t top = root_all->val;
	top |= calc_parity(top);

	*guess = top;
}

static void bail_out(int eval, const char *fmt, ...)
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
	exit(eval);
}

static void free_resources(void)
{
	sigset_t blocked_signals;
	(void) sigfillset(&blocked_signals);
	(void) sigprocmask(SIG_BLOCK, &blocked_signals, NULL);

	/* signals need to be blocked here to avoid race */
	if(terminating == 1) {
		return;
	}
	terminating = 1;

	/* clean up resources */
	DEBUG("Shutting down client\n");
	if(connfd >= 0) {
		(void) close(connfd);
	}
	if(sockfd >= 0) {
		(void) close(sockfd);
	}
}

static void signal_handler(int sig)
{
	/* signals need to be blocked by sigaction */
	DEBUG("Caught Signal\n");
	free_resources();
	exit(EXIT_SUCCESS);
}

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/**
 * @brief Program entry point
 * @param argc The argument counter
 * @param argv The argument vecotr
 * @return EXIT_SUCCESS on success, EXIT_PARITY_ERROR in case of an parity
 * error, EXIT_GAME_LOST in case client needed to many guesses,
 * EXIT_MULTIPLE_ERRORS in case multiple errors occurred in one round
 */
int main(int argc, char*argv[])
{

	sigset_t blocked_signals;

	/* Check arguments */
	if(argc < 3)
	{
		bail_out(EXIT_FAILURE, "Usage: %s <server-hostname> <server-port>", progname);
	}

	/* setup signal handlers */
	if(sigfillset(&blocked_signals) < 0) {
		bail_out(EXIT_FAILURE, "sigfillset");
	} else {
		const int signals[] = { SIGINT, SIGQUIT, SIGTERM };
		struct sigaction s;
		s.sa_handler = signal_handler;
		(void) memcpy(&s.sa_mask, &blocked_signals, sizeof(s.sa_mask));
		s.sa_flags   = SA_RESTART;
		for(int i = 0; i < COUNT_OF(signals); i++) {
			if (sigaction(signals[i], &s, NULL) < 0) {
				bail_out(EXIT_FAILURE, "sigaction");
			}
		}
	}

	/* Set up list with all possible combinations (Knuth) There are 8^5 possibilities*/
	root_all = root_sel = NULL;
	for(int i = 0; i < 32768; i++)
	{
		curr = (struct node*)malloc(sizeof(struct node*));
		curr->val = i;
		curr->next = root_all;
		root_all=curr;
	}

	/* Set up a connection with the server */
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int s;
	char resolve[30];

	memset (&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	
	/* We don't need to care about htons / how the bytes are arranged for the port, getaddrinfo does that work for us */
	/* Resolve hostname via gettaddrinfo */
	if ((s = getaddrinfo(argv[1], argv[2], &hints, &result)) != 0)
	{
		bail_out(EXIT_FAILURE, gai_strerror(s));
	}

	/* Loop through addrinfo structures, use the first where we succeed to stablish a connection */
	for(rp = result; rp != NULL; rp = rp->ai_next)
	{
		/* Create a socket */
		if((sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1)
		{
			perror("client: socket");
			continue;
		}

		/* Establish connection */
		if(connect(sockfd, rp->ai_addr, rp->ai_addrlen) == -1)
		{
			close(sockfd);
			perror("client: connect");
			continue;
		}

		/* If we didn't already hit a continue we are good to go and will have an open connection */
		break;
	}

	inet_ntop (rp->ai_family, get_in_addr((struct sockaddr *)rp->ai_addr), resolve, sizeof(resolve));

	/* Now the logic part */
	int proceed = 1;

	uint16_t send_buffer = 87; //00123 for the first guess
	uint8_t recv_buffer[1];
	char g_result[1]; //0 black, 1 whites


	while(proceed < 35)
	{
		proceed++;

		(void)write_to_server(sockfd, &send_buffer, WRITE_BYTES);
		fprintf(stdout, "Code that was sent: 0x%x\n", send_buffer);

		/* Read and interpret reply from server */
		(void)read_from_server(sockfd, &recv_buffer[0], READ_BYTES);
		
		g_result[0] = (recv_buffer[0] & 0x7);
		g_result[1] = (recv_buffer[0] >> 3) & 0x7;
		fprintf(stdout, "Black: %d | White: %d\n", g_result[0], g_result[1]);

		switch(recv_buffer[0]>>6)
		{
			case 1:
				bail_out(PARITY_ERR_BIT, "Parity Error\n");
				break;
			case 2:
				bail_out(GAME_LOST_ERR_BIT, "Game Lost\n");
				break;
			case 3:
				bail_out(EXIT_MULTIPLE_ERRORS, "Multiple Errors did occour\n");
				break;
			default:
				break;
		}
		/* No errors, now minimize our list of possible guesses based on our last guess and the answer (Knuth) */
		(void)remove_guesses(&recv_buffer[0], &send_buffer);
		(void)get_next_guess(&send_buffer);

	}
	
}
