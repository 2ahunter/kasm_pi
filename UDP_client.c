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
    int              sfd, s;
    char             buf[BUF_SIZE];
    union CMD_DATA buf_data;
    size_t           len;
    ssize_t          nread;
    struct addrinfo  hints;
    struct addrinfo  *result, *rp; // pointers to linked list of addrinfo structures
   
    if (argc < 3) {
        fprintf(stderr, "Usage: %s host port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Obtain address(es) matching host/port. */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM; // Datagram socket 
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          // Any protocol 

    s = getaddrinfo(argv[1], argv[2], &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket
       and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                     rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  // Success

        close(sfd);
    }

    freeaddrinfo(result);           // No longer needed
    if (rp == NULL) {               // No address succeeded
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }

    //set send and receive timeouts for the socket
    struct timeval timeout;      
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    if (setsockopt (sfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                sizeof timeout) < 0)
        printf("setsockopt failed\n");
        exit(EXIT_FAILURE);

    if (setsockopt (sfd, SOL_SOCKET, SO_SNDTIMEO, &timeout,
                sizeof timeout) < 0)
        printf("setsockopt failed\n");
        exit(EXIT_FAILURE);

    // populate the buffer with random values
    for(int i = 0; i < CMD_SIZE/2; i++){
        int16_t value = (rand() % 0xFFFF)>>8; // generate small random values
        cmd_data.values[i] = value;
        buf_data.values[i] = htons(value);
    }

    /* print the values*/
    for(int i = 0; i < CMD_SIZE/2; i++){
        printf("Value %d: %d\n", i, cmd_data.values[i]);
    }

    /* send command buffer values */
    len = CMD_SIZE;

    if (send(sfd, buf_data.bytes, len,0) != len) {
        fprintf(stderr, "partial/failed write\n");
        exit(EXIT_FAILURE);
    }

    // nread = recv(sfd, buf_data.bytes, CMD_SIZE,0);
    // if (nread == -1) {
    //     perror("read");
    //     exit(EXIT_FAILURE);
    // }

    // /* convert buffer data to host byte order */

    // printf("Received %zd bytes\n", nread);
    // for (size_t i = 0; i < nread/2; i++) {
    //     buf_data.values[i] = ntohs(buf_data.values[i]);
    //     printf("Received value %zu: %d\n", i, buf_data.values[i]);
    // }
    printf("Sent %zu bytes\n", len);
    exit(EXIT_SUCCESS);
}