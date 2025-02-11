#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define BACKLOG 10
#define MESSAGE_LEN 1024

void sigchld_handler(int s) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char* argv[]) {

    if (argc != 2) {
        fprintf(stderr, "usage: logger port\n");
        exit(1);
    }

    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    int rv;

    // get address for local host
    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1); 
    }

    int sockfd, yes = 1;
    // find available result
    for(p = servinfo; p != NULL; p=p->ai_next) {
        // try to create a socket
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            close(sockfd);
            perror("server: socket");
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("server: bind");
            continue;
        }
        break;
    }

    // check if successfully bind
    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(servinfo);

    // listen to the socket
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(2);
    }

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char addr_string[INET6_ADDRSTRLEN];
    int newfd;
    while (1) {
        sin_size = sizeof their_addr;
        newfd = accept(sockfd, (struct sockaddr*)&their_addr, &sin_size);
        if (newfd == -1) {
            perror("accept");
            continue;
        }
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr*)&their_addr),
            addr_string, sizeof addr_string);
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        double connection_time = ts.tv_sec + ts.tv_nsec / 1e9;
        
        int got_name = 0;
        if (!fork()) {
            close(sockfd);
            int bytes_received;
            char msg[MESSAGE_LEN];
            while ((bytes_received = recv(newfd, msg, MESSAGE_LEN - 1, 0)) > 0) {
                msg[bytes_received] = '\0';
                if (!got_name) {
                    got_name = 1;
                    printf("%.6f - %s connected\n", connection_time, msg);
                    continue;
                }
                printf("%s\n", msg);
                fflush(stdout); 
            }
            close(newfd);
            exit(0);
        }
        close(newfd);
    }
    return 0;
}