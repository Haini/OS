/**
 *  @file common_hangman.h
 *  @author Constantin Schieber, e1228774
 *  @brief Contains common Values / Defines for client and server 
 *  @details Every data structure that gets used in client and server 
 *  @date 12.01.2016
 * */

#define MAX_DATA (50)
#define MAX_INPUT_PER_LINE (80)
#define MAX_ERRORS (9)
#define PERMISSION (0600)
#define ALPHABET_CNT (26)

#define SHM_NAME ( "/shm_name" ) 
#define S_SERVER ( "/s_server" )
#define S_CLIENT ( "/s_client" )
#define S_RETURN ( "/s_return" )

#define COUNT_OF(x) (sizeof(x)/sizeof(x[0]))

enum StatusID {
   CreateGame,
   CreatedGame,
   Running,
   Won,
   Lost,
   EndGame,
};

struct Myshm {
    int s_id;                  /* ID of the player. */
    uint8_t s_errors;          /* Amount of errors that were already made. */
    char s_word[MAX_DATA];     /* The partly correct word. */
    char c_guess;              /* New guess by client. */
    char sc_terminate;         /* Server / Client are terminating. */
    uint8_t status_id;         /* Tells us the State of the client. */
};

