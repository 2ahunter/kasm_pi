#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUF_SIZE 500
#define CMD_SIZE 52 // 52 bytes for 26 int16_t values

union CMD_DATA {
    unsigned char bytes[CMD_SIZE];
    int16_t values[CMD_SIZE/2];
} cmd_data, buf_data;

int
main(int argc, char *argv[])
{
    int                      sfd, s;
    char                     buf[BUF_SIZE];
    ssize_t                  nread;
    socklen_t                peer_addrlen;
    struct addrinfo          hints;
    struct addrinfo          *result, *rp;
    struct sockaddr_storage  peer_addr;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    s = getaddrinfo(NULL, argv[1], &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully bind(2).
       If socket(2) (or bind(2)) fails, we (close the socket
       and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                     rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;                  /* Success */

        close(sfd);
    }
    freeaddrinfo(result);           /* No longer needed */

    if (rp == NULL) {               /* No address succeeded */
        fprintf(stderr, "Could not bind\n");
        exit(EXIT_FAILURE);
    }

    /* Read datagrams and echo them back to sender. */

    for (;;) {
        char host[NI_MAXHOST], service[NI_MAXSERV];

        peer_addrlen = sizeof(peer_addr);
        nread = recvfrom(sfd, cmd_data.bytes, CMD_SIZE, 0,
                         (struct sockaddr *) &peer_addr, &peer_addrlen);
        if (nread == -1)
            continue;               /* Ignore failed request */

        s = getnameinfo((struct sockaddr *) &peer_addr,
                        peer_addrlen, host, NI_MAXHOST,
                        service, NI_MAXSERV, NI_NUMERICSERV);
        if (s == 0) {
            printf("Received %zd bytes from %s:%s\n",
                   nread, host, service);
            for (size_t i = 0; i < nread/2; i++) {
                buf_data.values[i] = cmd_data.values[i];
                cmd_data.values[i] = ntohs(cmd_data.values[i]);
                printf("Received value %zu: %d\n", i, cmd_data.values[i]);
            }
        }
        else
            fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));

        if (sendto(sfd, buf_data.bytes, nread, 0, (struct sockaddr *) &peer_addr,
                   peer_addrlen) != nread)
        {
            fprintf(stderr, "Error sending response\n");
        }
    }
}
