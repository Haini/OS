/*
 * Copyright (c) 2014 Constantin Schieber <e1228774@student.tuwien.ac.at>
 *
 *	* Name of Module: mycompress.c
 *	 * @author Constantin Schiber, e1228774
 *	  * @brief Compresses a given input stream (stdin, file) and creates a new file of the form xxx.txt.comp with the output
 *	   * @details 
 *	    * @date 02.11.2014
 *	     */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* === Constants === */

const int SIGN_MAX = 9;


/* === Global Variables === */

/* Name of the program */
char* pgm_name;

/* Input and output stream */
FILE *in_stream, *out_stream;

/* === Prototypes === */

/**
 *	* @brief Closes streams when the program terminates
 *	 */
void cleanup (void);

/**
 *	* @brief Prints the Synopsis for calling the program
 *	 */
void usage (void);

/**
 *	* @brief Reads the stream, compresses while reading and writes to the output stream
 *	 * @details Reads the given input stream char by char, counts the occourences of the same char and writes the amount of
 *	  * occurences to the output stream when the next char is different from the previous.
 *	   * @param A reference to the int that counts the amount of chars of the original file
 *	    * @param A reference to the int that counts the amount of chars of the compressed file
 *	     */
void compress (int*, int*);

/**
 *	* @brief Opens the input and output stream
 *	 * @details
 *	  * @param A reference to the name of the input stream
 *	   * @param A reference to the name of the output stream
 *	    */
void open_stream (char* , char* );

/*In Out CcIn CcOut*/

/**
 *	* @brief Formats and prints the results of the compression
 *	 * @details
 *	  * @param A reference to the name of the input stream
 *	   * @param A reference to the name of the output stream
 *	    * @param The amount of chars in the original file
 *	     * @param The amount of chars in the compressed file
 *	      */
void output_summary (char*, char*, int, int);



/**
 *	* @brief Program entry point
 *	 * @param argc The argument counter
 *	  * @param argv The argument vector
 *	   * @return EXIT_SUCCESS on success, EXIT_ERROR in case of an error
 *	    */
int main(int argc, char** argv)
{
	/* Define a subroutine where the program jumps to if it exits gracefully */
	(void) atexit (cleanup);

	/* Input / Output Character Count */
	int in_ccount, out_ccount;	
	
	/* Save the name of the program */
	pgm_name = argv[0];
	

	/* Program is called with arguments */
	if (argc > 1)
	{	
		for (int i = 1; i < argc; i++)
		{
			char* out_name = malloc(sizeof(char)* (strlen(argv[i])+5));	//Allocate memory for the name of the output string
			(void) open_stream (argv[i], out_name);
			(void) compress (&in_ccount, &out_ccount);
			(void) output_summary (argv[i], out_name, in_ccount, out_ccount);
			free(out_name);
		}
	 }
	 else
	 {
		 char* out_name = malloc(sizeof(char)* (strlen("stdin.txt")+5));	//Allocate memory for the name of the output string
		 (void) open_stream ("stdin", out_name );
		 (void) compress (&in_ccount, &out_ccount);
		 (void) output_summary ("stdin", out_name, in_ccount, out_ccount);
		 free(out_name);
	 }

	 exit (EXIT_SUCCESS);
}


void cleanup (void)
{
	if (in_stream != NULL) 
	{
		fclose (in_stream);
	}
	if (out_stream != NULL)
	{
		fclose (out_stream);
	}
}


void usage (void)
{
	(void) fprintf(stderr, "Usage: %s [file1] [file2] ...\n", pgm_name);
	exit (EXIT_FAILURE);
}


void open_stream (char* in_name, char* out_name)
{
	/* If we have a stdin as input we need to handle it in another way */		
	if (strcmp(in_name, "stdin") != 0) 
	{
		if ((in_stream = fopen(in_name, "r")) == NULL)
		{
			usage();	//The only case where the user could benefit from a usage() call
			exit (EXIT_FAILURE);
		}

	}
	else
	{
		in_stream = stdin;
	}


	/* Rename the output file from xxx.txt to xxx.txt.comp */
	if ((strcpy (out_name, in_name)) == NULL)
	{
		(void) fprintf(stderr, "%s: Error while copying strings: %s\n", pgm_name, strerror (errno));
		exit (EXIT_FAILURE);
	}

	if ((strcat (out_name, ".comp")) == NULL)
	{
		(void) fprintf(stderr, "%s: Error while concatenating strings: %s\n", pgm_name, strerror (errno));
		exit (EXIT_FAILURE);
	}

	
	/* Output file is always the same */
	if ((out_stream = fopen(out_name, "w")) == NULL)
	{
		(void) fprintf(stderr, "%s: Error while opening stream: %s\n", pgm_name, strerror (errno));
		//Couldn't open stream
		exit (EXIT_FAILURE);
	}
}


void compress (int* in_ccount, int* out_ccount)
{
	int x, prev_x=NULL, count_x=1;
	int in_length = 0, out_length = 0;

	/* Read the input stream char by char and count how often the char occours consecutive. Always save the previous char.
	 * If the chars are not equal write the current sign and its number of occurrences to the output stream
	 */
	while ((x = fgetc (in_stream)) != EOF)
	{
		if (in_length > 0)
		{
			if(prev_x == x)
			{
				count_x++;
			}
			else
			{
				if ((fputc(prev_x, out_stream)) == EOF)
				{
					(void) fprintf(stderr, "%s: Error while writing to stream: %s\n", pgm_name, strerror (errno));
					
					//Could not write to file
					exit (EXIT_FAILURE);
				}

				if ((fprintf(out_stream, "%d", count_x)) == EOF)
				{
					
					(void) fprintf(stderr, "%s: Error while writing to stream: %s\n", pgm_name, strerror (errno));

					//Could not write to file
					exit (EXIT_FAILURE);
				}
				
				prev_x = x;
				count_x = 1;
				out_length+=2;
			}
		}
		else
		{
			prev_x = x;
		}

		in_length++;
				
		/* The maximal amount of signs in one line is defined as the constant LINE_MAX, but since we can assume that no
		 * line contains more than LINE_MAX signs (which could be 0 too) no checks are done.
		 */
		
	}
	
	*in_ccount = in_length;
	*out_ccount = out_length;

	if ((fputc(prev_x, out_stream)) == EOF)
	{
		(void) fprintf(stderr, "%s: Error while writing to stream: %s\n", pgm_name, strerror (errno));
		
		//Could not write to file
		exit (EXIT_FAILURE);
	}
	
	if ((fprintf(out_stream, "%d", count_x)) == EOF)
	{
		(void) fprintf(stderr, "%s: Error while writing to stream: %s\n", pgm_name, strerror (errno));
	
		//Could not write to file
		exit (EXIT_FAILURE);
	}

}


void output_summary (char* in_name, char* out_name, int in_ccount, int out_ccount)
{
	int offset = strlen( out_name ) + 2; /*< Because of ': ' + 2*/
	
	char *in = (char *) malloc(sizeof(char)*offset);
	if (in == NULL) 
	{
		(void) fprintf(stderr, "%s: Error while allocating memory: %s\n", pgm_name, strerror (errno));
	}
	
	//We don't need to check the return value of memcpy since it only returns a pointer to dest, same goes for strcat
	(void) memcpy(in , in_name, offset);	
	(void) strcat( in, ": " );

	char *out = (char *) malloc(sizeof(char)*offset);
	if (out == NULL)
	{
		(void) fprintf(stderr, "%s: Error while allocating memory: %s\n", pgm_name, strerror (errno));
	}
	(void) memcpy(out , out_name, offset);
	(void) strcat( out, ": " );

	(void) fprintf(stdout, "%-*s%-d\n%-*s%-d\n", offset, in, in_ccount,
			offset, out,
			out_ccount);

	free (in);
	free (out);
}
