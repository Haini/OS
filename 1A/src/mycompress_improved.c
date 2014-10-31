#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Gloabl Vairables */
FILE *in_stream, *out_stream;
char* pgm_name;
const int SIGN_MAX = 9;

/* Function headers */
void cleanup (void);


void usage (void);


void compress (int*, int*);


void open_stream (char* , char* );

/*In Out CcIn CcOut*/
void output_summary (char*, char*, int, int);



/* Actual Program */

int main(int argc, char** argv)
{
	
	(void) atexit (cleanup);

	int in_ccount, out_ccount;	
	
	pgm_name = argv[0];
	

	/* Program is called with arguments */
	if (argc > 1)
	{	
		for (int i = 1; i < argc; i++)
		{
					
			char* out_name = malloc(sizeof(char)* (strlen(argv[i])+5));
			(void) open_stream (argv[i], out_name);
			(void) compress (&in_ccount, &out_ccount);
			(void) output_summary (argv[i], out_name, in_ccount, out_ccount);
			free(out_name);
		}
	 } 
	 else
	 {
		 char* out_name = malloc(sizeof(char)* (strlen("stdin.txt")+5));
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
	(void) fprintf(stderr, "Usage: %s [file1] [file2] ...", pgm_name);
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

	} else
	{
		in_stream = stdin;
		
	}

	


	/* Rename the output file from xxx.txt to xxx.txt.comp */
	if ((strcpy (out_name, in_name)) == NULL)
	{
		exit (EXIT_FAILURE);
	}

	if ((strcat (out_name, ".comp")) == NULL)
	{
		exit (EXIT_FAILURE);
	}

	
	/* Output file is always the same */
	if ((out_stream = fopen(out_name, "w")) == NULL)
	{
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
					//Could not write to file
					exit (EXIT_FAILURE);
				}

				if ((fprintf(out_stream, "%d", count_x)) == EOF)
				{
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
		//Could not write to file
		exit (EXIT_FAILURE);
	}
	
	if ((fprintf(out_stream, "%d", count_x)) == EOF)
	{
		//Could not write to file
		exit (EXIT_FAILURE);
	}

}


void output_summary (char* in_name, char* out_name, int in_ccount, int out_ccount)
{
	

	int offset = strlen( out_name ) + 2; /*< Because of ': ' + 2*/
	char *in = (char *) malloc(sizeof(char)*offset);
	memcpy(in , in_name, offset);
	strcat( in, ": " );
	char *out = (char *) malloc(sizeof(char)*offset);
	memcpy(out , out_name, offset);
	strcat( out, ": " );

	fprintf(stdout, "%-*s%-d\n%-*s%-d\n", offset, in, in_ccount,
			offset, out,
			out_ccount);
	
}
