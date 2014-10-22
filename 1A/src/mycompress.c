/**
 * @file mycompress.c
 * @author Constantin Schieber <e1228774@tuwien.ac.at>
 * @date 20.10.2014
 *
 * @brief Main program module
 *
 * This program accepts *.txt files or a stdin stream and compresses them. The output is then written to a *.comp file
 **/

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define TERMINATE_ON_ERR(pgm_name, text) { (void)fprintf( stderr, "%s: %s\n", pgm_name, text ); exit(1);}

struct s_io_information
{
		char *in_name;
		char *out_name;
		int in_ccount;
		int out_ccount;
};

extern int errno; /**< global error number used by errno.h */

static char *pgm_name; /**< The program name.*/


/**
 * Mandatory usage function
 * @brief This function writes helpful usage information about the program to stderr
 * @details global variables: pgm_name
 * @param
 * @return
 */
static void usage (void) {
		(void) fprintf(stderr, "USAGE: %s [file1] [file2] ...\n", pgm_name);
		exit(EXIT_FAILURE);
}

/**
 * Input processing function
 * @brief This function reads the current stream, processes it and puts the output in a new file
 * @details
 * @param
 * @return
 */
static struct s_io_information process_input( FILE* in_stream, char* out_filename )
{
		int x, prev_x=NULL, count_x = NULL;
		const char *mode = "w";
		int in_length = 0, out_length = 0;
		
		struct s_io_information io_information;

		FILE *out_stream = fopen( out_filename, mode );

		if( out_stream == 0 )
		{
				//(void) fprintf( stderr, "%s: Cannot create output file. Program can't finish execution\n", pgm_name );
				TERMINATE_ON_ERR(pgm_name, "Cannot create output file. Program can't finish execution");
				fclose( out_stream );
				fclose( in_stream );

				exit(1);
		}

		while (( x = fgetc( in_stream )) != EOF )
		{
				if( prev_x == 0 || prev_x == x )
				{
						prev_x = x;
						count_x++;
				} 
				else
				{
						(void) fputc( prev_x, out_stream );
						(void) fprintf( out_stream, "%d", count_x );
						prev_x = x;
						count_x = 1;
						out_length+=2;
				}
				in_length++;
		}
		if(count_x > 0) {
				(void) fputc( prev_x, out_stream );
				(void) fprintf( out_stream, "%d", count_x );
		}
		prev_x = 0;
		count_x = 0;

		io_information.in_ccount = in_length;
		io_information.out_ccount = out_length;
		io_information.out_name = out_filename;

		if( out_stream != NULL )
				(void)fclose( out_stream );
		if( in_stream != NULL )
				(void)fclose( in_stream );

		return io_information;
}

/**
 * Renaming function
 * @brief This function reads a passed char* of the form xxx.txt and renames it to xxx.comp
 * @details
 * @param char* out_name contains the char* that needs to be renamed
 * @return char* out_newname contains the renamed char*
 */
static char* rename_file( char* out_name )
{

		char* out_newname = malloc( strlen( out_name ) + 5 ); /*<Allocate memory for the new string. +5 because of .comp */

		if( out_newname == NULL ) 
		{
				TERMINATE_ON_ERR(pgm_name, "Cannot allocate memory for renaming. Program can't finish execution");
		}

		(void) strcat( out_newname, out_name );

		(void) strcat( out_newname, ".comp" );

		return out_newname;
}

/**
 * Provides a summary of the compressing results
 * @brief Puts the name of the in and out files with their char count on the screen
 * @details
 * @param
 */
static void summary(struct s_io_information io_out)
{
		int offset = strlen( io_out.out_name ) + 2; /*< Because of ': ' + 2*/
		char *in = (char *) malloc(sizeof(char)*offset);
		memcpy(in , io_out.in_name, offset);
		strcat( in, ": " );
		char *out = (char *) malloc(sizeof(char)*offset);
		memcpy(out , io_out.out_name, offset);
		strcat( out, ": " );

		fprintf(stdout, "%-*s%-d\n%-*s%-d\n", offset, in, io_out.in_ccount,
											offset, out, 
											io_out.out_ccount);
		//free(in);
		//free(out);
}

/**
 * Program entry point.
 * @brief The program starts here.
 * @details Non yet.
 * global variables: pgm_name
 * @param argc The argument counter.
 * @param argv The argument vector.
 * @return Returns EXIT_SUCCESS in case of success.
 */

int main(int argc, char **argv) {

		pgm_name = argv[0];

		int c, offset;
		char *in_filename = "stdin.txt";
		FILE *in_stream = stdin;	
		struct s_io_information io_information;
		

		if(argc > 1)
		{
			for(int i = 1; i < argc; i++)
			{
				in_filename = (char *) malloc(sizeof(argv[i]));	
				strcpy( in_filename , argv[i] );

				in_stream = fopen( in_filename, "r");

				if( in_stream == 0 )
				{
					fprintf( stderr, "%s: Could not open file '%s'\n", pgm_name, in_filename );
					exit(1);
				}
				
				io_information = process_input( in_stream, rename_file( in_filename ));
				io_information.in_name = in_filename;
				
				summary( io_information );

				free(in_filename);
				free(io_information.out_name);
			}
		 }
		 else
		 {
		 	io_information = process_input( in_stream, rename_file( in_filename ));
			io_information.in_name = in_filename;	
			summary( io_information );
		 }
		
		exit(EXIT_SUCCESS);
}
