#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <syslog.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <errno.h>
#include "UDP_client.h"

#define	TRUE	(1==1)
#define	FALSE	(!TRUE)
#define BUFFER_SIZE 96
#define PERIOD_NSEC 500000// UDP_send interval in nanoseconds
#define NSEC_PER_SEC 1000*1000*1000

/********** Module level variables ***************************/
extern int UDP_fd; // file descriptor for the UDP socket, set by UDP_init()
uint8_t running = TRUE; // set flag to false to terminate the threads and exit the program
char ip_addr[80] = "128.114.22.117";
char port[20] = "5001";
int num_cmds = 1; // number of commands to send, default is 1


/********** functions ***************************************/
/**
 * @brief Normalize timer to account for seconds rollover
 * @param timespec_t ts Pointer to timespec structure to normalize
 */
static void normalize_timespec(struct timespec *ts) {
    while (ts->tv_nsec >= NSEC_PER_SEC) {
        ts->tv_sec += 1;
        ts->tv_nsec -= NSEC_PER_SEC;
    }
}

void * recv_UDP(void *data){
    ssize_t nread;
    socklen_t peer_addrlen;
    struct sockaddr_storage peer_addr;
    char buf_data[BUFFER_SIZE]={0}; // buffer for UDP data
    int timeout_ms = 1; 
    const char *filename = "testing/UDP_client_test_data.csv";

    peer_addrlen = sizeof(peer_addr);
    // set up polling
    struct pollfd fds[1]; //monitor UDP for incoming data
    fds[0].fd = UDP_fd;
    fds[0].events = POLLIN; // look for new data
    int poll_ret = {0};

    /* datalogging of returns values */
    FILE *fp = fopen(filename, "w"); // open file for writing
    if (fp == NULL) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    while(running == TRUE){
        poll_ret = poll(fds, 1, timeout_ms);
        if(poll_ret < 0) exit(EXIT_FAILURE); // error
        if(poll_ret == 0) {
            syslog(LOG_WARNING, "UDP poll timeout after %d ms", timeout_ms);
        } else {
            // Receive data from the UDP socket
            nread = recvfrom(UDP_fd, buf_data, BUFFER_SIZE, 0,
                            (struct sockaddr *)&peer_addr, &peer_addrlen);
            if (nread == -1)
            {
                syslog(LOG_ERR, "Error receiving UDP data: %s\n", strerror(errno));
            }

            else {
                syslog(LOG_INFO, "Received %zd bytes", nread);
                buf_data[nread] = '\0'; // null-terminate the response
                fprintf(fp, "%s\r\n", buf_data); // write data to datalog
            }
        }
    }
    fclose(fp); // close the datalog file
    pthread_exit(NULL); // Return NULL to indicate thread completion

}

void * send_UDP(void *data){
    struct timespec prd_tmr={0};
    struct timespec curr_tmr={0};
    long int delta_time_nsec = {0};
    int bytes_sent = {0};
    union CMD_DATA cmd_data, buf_data;

        // populate the buffer with random values
    for(int i = 0; i < CMD_SIZE/2; i++){
        int16_t value = (rand() % 0xFFFF)>>8; // generate small random values
        cmd_data.values[i] = value;
        buf_data.values[i] = htons(value);
    }

    for(int i = 0; i < num_cmds; i++){
        clock_gettime(CLOCK_MONOTONIC, &prd_tmr); // get the current time for the period timer
        /* use index for the command values*/
        for(int j = 0; j < CMD_SIZE/2; j++){
            buf_data.values[j] = htons(i);
        }

        /* send command buffer values */
        bytes_sent = UDP_send(buf_data);
        syslog(LOG_DEBUG, "Sent %zu bytes, iter %d of %d\n", (size_t)bytes_sent,i,num_cmds);

        // Calculate next wake-up time
        prd_tmr.tv_nsec += PERIOD_NSEC;
        normalize_timespec(&prd_tmr);
        // Get the current time for logging
        clock_gettime(CLOCK_MONOTONIC, &curr_tmr);
        // Compute the difference between current time and next period time
        delta_time_nsec = (prd_tmr.tv_sec - curr_tmr.tv_sec) * NSEC_PER_SEC +
                            (prd_tmr.tv_nsec - curr_tmr.tv_nsec);
        if (delta_time_nsec < 0) {
            syslog(LOG_ERR, "Missed deadline by %ld ns", -delta_time_nsec);
            // If we missed the deadline 
            // set period timer to current time plus one period
            prd_tmr.tv_sec = curr_tmr.tv_sec;
            prd_tmr.tv_nsec = curr_tmr.tv_nsec;
            prd_tmr.tv_nsec += PERIOD_NSEC;
            normalize_timespec(&prd_tmr);
        }
        syslog(LOG_INFO, "Sleep until: %ld.%09ld", prd_tmr.tv_sec, prd_tmr.tv_nsec);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &prd_tmr, NULL);

    }
    running = FALSE; // set running to false to signal the recv thread to exit
    pthread_exit(NULL); // Return NULL to indicate thread completion
}

int main(int argc, char *argv[])
{
    int opt = {0}; // for getopt()

    /*  parse command line arguments*/
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

    /*  open syslog for logging */
    openlog(NULL, LOG_PID, LOG_LOCAL6); // Open syslog for logging
    int mask = LOG_MASK(LOG_INFO) | LOG_MASK(LOG_ERR) | LOG_MASK(LOG_NOTICE);
    setlogmask(mask);

    /*  initialize UDP socket */
    UDP_fd = UDP_init(ip_addr, port);

    if(UDP_fd<0){
        syslog(LOG_ERR, "Failed to get socket descriptor");
        exit(EXIT_FAILURE);
    } else {
        syslog(LOG_INFO, "UDP client initialized with socket descriptor %d", UDP_fd);
    }

    /* start UDP listener thread */
    pthread_t udp_recv_thread;
    if(pthread_create(&udp_recv_thread, NULL, recv_UDP, NULL) != 0){
        syslog(LOG_ERR, "Failed to create UDP listener thread: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    /* start UDP send thread */
    pthread_t udp_send_thread;
    if(pthread_create(&udp_send_thread, NULL, send_UDP, NULL) != 0){
        syslog(LOG_ERR, "Failed to create UDP send thread: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    pthread_join(udp_recv_thread, NULL);
    pthread_join(udp_send_thread, NULL);
    syslog(LOG_INFO, "UDP client exiting");
    printf("UDP client exiting\n");

    exit(EXIT_SUCCESS);
}