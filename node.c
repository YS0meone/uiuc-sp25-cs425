#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>
#include <arpa/inet.h>

#define HASH_LEN 100
#define BUFFER_LEN 200
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	if (argc != 4) {
	    fprintf(stderr,"usage: node name ip port\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[2], argv[3], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

    
    char buffer[BUFFER_LEN];
    snprintf(buffer, BUFFER_LEN, "%s", argv[1]);
    printf("the node name is %s\n", buffer);
    if (send(sockfd, buffer, strlen(buffer), 0) == -1) {
        perror("send fail");
    }
    
    char event_hash[HASH_LEN];
    double timestamp;
    while (scanf("%lf %99s", &timestamp, event_hash) == 2) {
        snprintf(buffer, BUFFER_LEN, "%.6f %s %s", timestamp, argv[1], event_hash);  // ✅ Safe formatting

        ssize_t total_sent = 0;
        ssize_t bytes_left = strlen(buffer);
        ssize_t n;

        while (total_sent < bytes_left) {
            n = send(sockfd, buffer + total_sent, bytes_left - total_sent, 0);
            if (n == -1) {
                perror("send fail");
                break;
            }
            total_sent += n;
        }
    }

    close(sockfd);
    return 0;
}

