/*
 * @file client.c
 * @author Constantin Schieber, e1228774
 * @brief Client that plays Mastermind with a server
 * @details Connects to a given address and plays Mastermind against a server
 * @date 03.11.2014
 * */


#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>


/* === Constants === */
#define SLOTS (5)
#define COLORS (8)
#define MAX_TRIES (35)
#define MAXDATASIZE (50)
#define WRITE_SIZE (2)
#define READ_SIZE (1)
#define PARITY_ERR_BIT (6)
#define GAME_LOST_ERR_BIT (7)
#define SHIFT_WIDTH (3)
/* === Macros === */

/* === Global Variables === */

/* Name of the program */
static const char *progname = "client"; /* default name */

/* File descriptor for client socket */
static int sockfd = -1;

/* File descriptor for connection socket */
static int connfd = -1;

/* This variable is set to ensure cleanup is performed only once */
//volatile sig_atomic_t terminating = 0;

/* === Type Definitions === */

/* === Prototypes === */


/**
 *  @brief Prints how to call this program
 */
static void usage(void);


/**
 *  @brief Computes how many black and whites are in the guess
 *  @param huess The guess of my program
 *  @param result Reference to to a result
 *  @param secret The secret code my function checks with
 *  @return Returns amount of black and whites
 *  */
static void compute_answer(char* guess, char* result, char* secret);

/**
 *  @brief Sends the answer, calculates all the cool parity stuff
 */
static void send_answer(char* guess);

/**
 *  @brief Calculates a score based on how many blacks and whites are contained in my guess
 *  @param black Amount of blacks
 *  @param white Amount of whites
 *  @return Returns black and whites
 */
static int calculate_score(uint8_t black, uint8_t white);

/**
 *  @brief A counter that just counts in the octal number system
 *  @param num Contains the current counter value that has to be increased
 */
static void base8counter(char num[4]);


/* TEMP SHIT */

static void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* END TEMP SHIT */



/**
 * @brief Program entry point
 * @param argc The argument counter
 * @param argv The argument vector
 * @return EXIT_SUCCESS on success, EXIT_ERROR in case of an error
 * */
int main( int argc, char **argv )
{	
	/* There shall not be an other argc than 3 */
	if (argc != 3)
	{
		usage();
		exit (EXIT_FAILURE);
	}
	
	/* Establish a connection */
	int numbytes;
	//char buffer[MAXDATASIZE];
	struct addrinfo hints, *serverinfo, *p;
	int rv;
	//int round = 0;
	char s[30];

	memset (&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	/* Resolve hostname via getaddrinfo */
	if ((rv = getaddrinfo (argv[1], argv[2], &hints, &serverinfo)) != 0)
	{
		fprintf (stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit (EXIT_FAILURE);
	}

	/* Loop trough addrinfo structures, use the first where we succeed to establish a connection */
	for (p = serverinfo; p != NULL; p = p->ai_next) 
	{
		/* Create a socket */
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p-> ai_protocol)) == -1)
		{
			perror("client: socket");
			continue;
		}
		
		/* Establish a Connection */
		if (connect (sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close (sockfd);
			perror ("client: connect");
			continue;
		}
		
		/* If we didn't already hit a continue we are good to go and will have a open connection */
		break;
	}
	
	/* Well, something can go wrong all the time */
	if (p == NULL)
	{
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop (p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof(s));
	printf("client: connecting to %s\n", s);

	freeaddrinfo (serverinfo); //all done with this structure
	
	
	/* Now things get a little bit more serious, we need to send stuff to our server */
	
	/* 1. Make a guess */

	int round = 0;
	char result[] = {0,0};
	char g_code[SLOTS] = {0};
	char a_code[SLOTS] = {0};
	char solution[SLOTS];
	char checked[SLOTS] = {0};
	char incorrect[SLOTS];

	memset(solution, 255, SLOTS);
	memset(checked, 0, SLOTS);
	memset(incorrect, 255, SLOTS);

	/* Make my own initial guess */
	g_code[0] = 0; g_code[1] = 0; g_code[2] = 1; g_code[3] = 1; g_code[4] = 2;
	a_code[0] = 0; a_code[1] = 0; a_code[2] = 1; a_code[3] = 1; a_code[4] = 2;

	/* Arrays to remember previous guesses and scores, 15 is a randomly chosen value */
	char previous_answers[15][SLOTS];
	char previous_scores[15];

	while(round < 15)
	{
		round++;
		(void)fprintf(stdout, "Code: %d %d %d %d %d\n", g_code[0], g_code[1], g_code[2], g_code[3], g_code[4]);

		/* Send the code to the server */
		send_answer(&g_code[0]);
	
		/* Receive answer from server */
		char buffer[1];

		memset(buffer, 0, 2);

		size_t r = recv(sockfd, &buffer[0], 1, 0);
		
		if(r < 0)
		{
			exit (EXIT_FAILURE);
		}

		(void)fprintf(stdout, "Got byte 0x%x\n", buffer[0]);

		result[0] = buffer[0] & 0x7;	//Get black
		result[1] = (buffer[0] & 0x38) >> 3;	//Get white

		(void)fprintf(stdout, "Black sticks: 0x%x | White sticks: 0x%x\n", result[0], result[1]); 

		char score = calculate_score(result[0], result[1]);

		previous_answers[round - 1][0] = g_code[0];
		previous_answers[round - 1][1] = g_code[1];
		previous_answers[round - 1][2] = g_code[2];
		previous_answers[round - 1][3] = g_code[3];
		previous_answers[round - 1][4] = g_code[4];

		previous_scores[round -1] = score;

		if(result[0] == 4)
			break;

		int anySolution = 1;

		while (anySolution >= 1)
		{
			int consistent = 1;
			int i = 0;
			for(i = 0; i < round; i++)
			{
				compute_answer(a_code, result, previous_answers[i]);
				int test_score = calculate_score(result[0], result[1]);

				//fprintf(stdout, "TestScore %d | PrevScore %d\n", test_score, previous_scores[i]);
				if (test_score != previous_scores[i])
				{
					consistent = -1;
					break;
				}
			}

			if(consistent >= 1)
				break;

			base8counter(&a_code[0]);
		
			if(a_code[0] == 0 && a_code[1] == 0 && a_code[2] == 1 && a_code[3] == 1 && a_code[4] == 2)
			{
				anySolution = -1;
				fprintf(stdout, "No solution\n");
				exit (EXIT_FAILURE);
			}
		}
		
		memcpy(&g_code[0], &a_code[0], 5);

		fprintf(stdout, "\n My best guess: %d %d %d %d %d \n", g_code[0], g_code[1], g_code[2], g_code[3], g_code[4]);
	}

		if(result[0] == 4)
		{
			fprintf(stdout, "I win\n");
		}

}


static void usage(void)
{
	(void) fprintf(stderr, "Usage: %s <server-hostname> <server-port>", progname);
}

static void compute_answer(char* guess, char* result, char* secret)
{
	result[0] = 0;
	result[1] = 0;
	char colors_left[COLORS];
	//int guess[COLORS];
	int blacks, whites;
	int i, j;
	
	(void) memset(&colors_left[0], 0, sizeof(colors_left));
	blacks = whites = 0;
	/* Check for black sticks */
	for(i = 0; i < SLOTS; ++i)
	{
		if(guess[i] == secret[i])
		{
			blacks++;
		}
		else
		{
			colors_left[secret[i]]++; //Set index of secret to 1 to check back later
		}
	}

	/* Check for white sticks but ignore the ones that are already black */
	for(i = 0; i < SLOTS; ++i)
	{
		/* not marked as blacks */
		if (guess[i] != secret[i])
		{
			/* Check if this color did occour, if so then count whites up and remove the color from the pool */
			if (colors_left[guess[i]] > 0)
			{
				whites++;
				colors_left[guess[i]]--;
			}
		}
	}
	
	/* Pass the return value by reference*/
	result[0] = blacks;
	result[1] = whites;
}

static int calculate_score(uint8_t black, uint8_t white)
{
	if(black == 0 && white == 0) return 0;
	if(black == 0 && white == 1) return 1;
	if(black == 1 && white == 0) return 2;
	if(black == 0 && white == 2) return 3;
	if(black == 1 && white == 1) return 4;
	if(black == 2 && white == 0) return 5;
	if(black == 0 && white == 3) return 6;
	if(black == 1 && white == 2) return 7;
	if(black == 2 && white == 1) return 8;
	if(black == 3 && white == 0) return 9;
	if(black == 0 && white == 4) return 10;
	if(black == 1 && white == 3) return 11;
	if(black == 2 && white == 2) return 12;
	if(black == 4 && white == 0) return 13;

	if(black == 0 && white == 5) return 14;
	if(black == 1 && white == 4) return 15;
	if(black == 2 && white == 3) return 16;
	if(black == 3 && white == 2) return 17;
	if(black == 4 && white == 1) return 18;
	if(black == 5 && white == 0) return 19;

	return -1;
}

static void base8counter(char num[SLOTS])
{
	num[SLOTS-1]++;
	if(num[SLOTS-1] == 8)
	{
		num[SLOTS-2]++;
		if(num[SLOTS-2] == 8)
		{
			num[SLOTS-3]++;
			if(num[SLOTS-3] == 8)
			{
				num[SLOTS-4]++;
				if(num[SLOTS-4] == 8)
				{
					num[SLOTS-5]++;
					if(num[SLOTS-5] == 8)
					{
						num[SLOTS-5] = 0;
					}
					num[SLOTS-4] = 0;
				}
				num[SLOTS-3] = 0;
			}
			num[SLOTS-2] = 0;
		}
		num[SLOTS-1] = 0;
	}
}

static void send_answer(char* guess)
{
	/* To calculate the parity we xor all bits in 'guess'. The sign for xor is ^ */
	uint16_t parity_calc = 0;
	
	/* p c1 c2 c3 c4 c5 */
	uint16_t answer;
	


//	(void)fprintf(stdout, "Code: %d %d %d %d %d\n", guess[0], guess[1], guess[2], guess[3], guess[4]);

	/* convert my char array to uint16 for convenience */
	uint16_t conv_guess = 0;
	conv_guess <<= 15;
	conv_guess |= (guess[4] << 12); conv_guess |= (guess[3] << 9); conv_guess |= (guess[2] << 6); conv_guess |= (guess[1] << 3);
	conv_guess |= (guess[0]);
	uint16_t conv_guess2 = conv_guess;
	
//	fprintf(stdout, "My Guess: %x ... \n", conv_guess);

	/* All classic counter variable */
	int i;

	/* Do the parity bit calculation */
	for (i = 0; i < SLOTS; ++i)
	{
		int tmp = conv_guess & 0x07;					//Cut out three bits
		parity_calc ^=	tmp ^ (tmp >> 1) ^ (tmp >> 2);	//Do XOR on the three bits and the result from the past
		conv_guess >>= SHIFT_WIDTH;						//Shift out the three bits that have already been XORed
	}

	parity_calc &= 0x1;	//Oh Garry, why?

	parity_calc = parity_calc << 15; //Shift to the outer right because I am cool. P|lll mmm nnn vvv rrr
	
//	fprintf(stdout, "Parity bit: %x\n", parity_calc);

	answer = conv_guess2 | parity_calc; //Put together the pieces 
	
//	fprintf(stdout, "Try to send following code 0x%x ...\n", answer);

	if ((send(sockfd, &answer, 2, 0)) == -1) 	//Socket, *buf with content, 2 Bytes are sent, no flags are set
	{
		//DO CORRECT ERR HANDLING!
		perror ("Send:");
		exit (EXIT_FAILURE);
	}
//
	fprintf(stdout, "Sent the code successfully ... \n");
}
