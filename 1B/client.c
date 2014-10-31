#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main( int argc, char **argv )
{
	struct addrinfo hints;
	struct addrinfo *ai, *aip;
	int socketfd, res;

	memset( &hints, 0, sizeof hints );
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	res = getaddrinfo( "localhost", "1280", &hints, &ai );

	aip = ai;
	
	socketfd = socket( aip->ai_family, aip->ai_socktype, aip->ai_protocol );

	/* Connect the socket to the address */
	res = connect( socketfd, aip->ai_addr, aip->ai_addrlen );

}
