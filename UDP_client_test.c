#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include "UDP_client.h"

#define NOTHING

int main(int argc, char *argv[])
{
    int opt = {0}; // for getopt()
    int num_cmds = 1; 
    int fd; // socket file descriptor returned by UDP_init()
    union CMD_DATA cmd_data, buf_data;
    struct timespec end, start;
    double elapsed_time={0};
    ssize_t bytes_sent;
    char ip_addr[80] = "128.114.22.117";
    char port[20] = "5001";

    while ((opt = getopt(argc, argv, "s:hp:n:")) != -1){
        switch(opt){
            case 's':
                strcpy(ip_addr, optarg); 
                printf("set IP address to %s\n",ip_addr);
                break;
            case 'p':
                strcpy(port, optarg);
                printf("port set to %s\n", port);
                break;
            case 'n':
                num_cmds = atoi(optarg);
                printf("number of commands to send set to %d\n", num_cmds);
                break;
            case 'h':
                printf("Usage: %s host port [-n num cmds to send] \n", argv[0]);
                exit(EXIT_SUCCESS);
                break;
            default:
                printf("Usage: %s -s host -p port [-n num cmds to send] \n", argv[0]);
                exit(EXIT_FAILURE);
                break;
        }
    }

    fd = UDP_init(ip_addr, port);

    if(fd<0){
        fprintf(stderr, "Failed to get socket descriptor");
        exit(EXIT_FAILURE);
    } else {
        printf("UDP client initialized with socket descriptor %d\n", fd);
    }

    for(int i = 0; i < num_cmds; i++){
        clock_gettime(CLOCK_MONOTONIC, &start);
        // populate the buffer with random values
        for(int i = 0; i < CMD_SIZE/2; i++){
            int16_t value = (rand() % 0xFFFF)>>8; // generate small random values
            cmd_data.values[i] = value;
            buf_data.values[i] = htons(value);
        }
        /* send command buffer values */
        bytes_sent = UDP_send(buf_data);

        /* print the values*/
        // for(int i = 0; i < CMD_SIZE/2; i++){
        //     printf("Value %d: %d\n", i, cmd_data.values[i]);
        // }

        // printf("Sent %zu bytes\n", bytes_sent);

        do {
            clock_gettime(CLOCK_MONOTONIC, &end);
            elapsed_time = (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_nsec - start.tv_nsec) / 1e3; // convert to microseconds
        } while( elapsed_time < 500); // wait for 1ms before sending next sample
    }
     printf("Sent %zu bytes\n", bytes_sent*num_cmds);
    exit(EXIT_SUCCESS);
}