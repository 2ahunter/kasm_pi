#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "UDP_client.h"

#define NOTHING

int main(int argc, char *argv[])
{
    int fd; // socket file descriptor returned by UDP_init()
    union CMD_DATA cmd_data, buf_data;
    ssize_t bytes_sent;
    char *ip_addr;
    char *port;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s host port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    ip_addr = argv[1];
    port = argv[2];
    fd = UDP_init(ip_addr, port);

    if(fd<0){
        fprintf(stderr, "Failed to get socket descriptor");
        exit(EXIT_FAILURE);
    }

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
    bytes_sent = UDP_send(buf_data);

    printf("Sent %zu bytes\n", bytes_sent);
    exit(EXIT_SUCCESS);
}