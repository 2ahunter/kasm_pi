/**
 * @file knode.c
 * @brief Main file for the knode project.
 * @author Aaron Hunter
 * @date 2025-04-23
 * @details This file runs the kasm node, handling ethernet and SPI communication. 
 * It receives commands over UDP, processes them, and sends them to the KASM PCB via SPI.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <linux/spi/spidev.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <poll.h>
#include <pthread.h>

#include "wiringPi.h"
#include "wiringPiSPI.h"
#include "crc_check.h"
#include "timers.h"
#include "knode_thr.h"


/*************** defines **********************/
#define UDP_BUF_SIZE 500
#define CMD_SIZE 52 // 52 bytes for 26 int16_t values

#define	TRUE	(1==1)
#define	FALSE	(!TRUE)

#define SPI_DEV     1   // SPI device number
#define	SPI_CHAN	2   // which chip select
#define SPEED       5   // in megahertz
#define MHZ         1000000 // 1 MHz
#define SPI_BUF_SIZE    54 // bytes, including crc16
#define CRC_INDX    26  // index of the crc value in the data structure

#define MAX_VAL     24000
#define MIN_VAL     -24000

/********** module variables *****************/
uint8_t cmd_data_avail = FALSE; // flag to indicate if command data is available
uint8_t running = TRUE; // set flag to false to terminate the threads and exit the program
uint8_t main_run = TRUE;

union CMD_DATA {
    unsigned char bytes[SPI_BUF_SIZE];
    int16_t values[SPI_BUF_SIZE/2];
} cmd_data;


int spi_fd = {0}; // file descriptor for SPI
int udp_fd = {0}; // file descriptor for UDP
unsigned char TXRX_buffer[SPI_BUF_SIZE] = {0}; // buffer for SPI

// checksum parameters
uint16_t poly16 = {0x3D65}; // CRC-16-DNP polynomial
uint16_t init_val = {0xFFFF}; // initial value for CRC calculations
uint16_t crc={0}; // variable for CRC calculation

// mutex for thread synchronization
pthread_mutex_t mutex;


/********** functions *********************/

/**
 * @brief Initialize the UDP server
 * 
 * @param port Port number to bind to
 * @return int Socket file descriptor   
 */
int init_UDP(char *port){
    int sfd, s;
    struct addrinfo hints;
    struct addrinfo *result, *rp;

    // Initialize the hints structure
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM; // Datagram socket
    hints.ai_flags = AI_PASSIVE;    // For wildcard IP address
    hints.ai_protocol = 0;          // Any protocol
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    // Get address info
    s = getaddrinfo(NULL, port, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }

    // Try each address until we successfully bind(2)
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                     rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break; /* Success */

        close(sfd);
    }

    freeaddrinfo(result); // No longer needed

    if (rp == NULL) { /* No address succeeded */
        fprintf(stderr, "Could not bind\n");
        return -1;
    }

    return sfd;
}

/**
 * @brief Receive data over UDP
 * @return 0 on success, -1 on failure
 */
void * recv_UDP(void *data){

    ssize_t nread;
    socklen_t peer_addrlen;
    struct sockaddr_storage peer_addr;
    union CMD_DATA buf_data;
    memset(buf_data.bytes, 0, SPI_BUF_SIZE); // clear the UDP buffer

    peer_addrlen = sizeof(peer_addr);
    // set up polling
    struct pollfd fds[1]; //monitor UDP for incoming data
    fds[0].fd = udp_fd;
    fds[0].events = POLLIN; // look for new data
    int poll_ret = {0};

    while(running == TRUE){
        poll_ret = poll(fds, 1, 0);
        if(poll_ret < 0) exit(EXIT_FAILURE); // error
        if(poll_ret > 0) {
            syslog(LOG_NOTICE, "UDP data available");
            // Receive data from the UDP socket
            nread = recvfrom(udp_fd, buf_data.bytes, CMD_SIZE, 0,
                            (struct sockaddr *)&peer_addr, &peer_addrlen);
            if (nread == -1)
            {
                syslog(LOG_ERR, "Error receiving UDP data: %s\n", strerror(errno));
            }

            if (nread == CMD_SIZE) {
                syslog(LOG_DEBUG, "Received %zd bytes", nread);
                pthread_mutex_lock(&mutex); // lock the data
                for (size_t i = 0; i < nread / 2; i++) {
                    cmd_data.values[i] = ntohs(buf_data.values[i]);
                    syslog(LOG_DEBUG, "Received value %zu: %d\n", i, buf_data.values[i]);
                }
                crc = append_crc(); // compute the crc and append to the command data
                pthread_mutex_unlock(&mutex); // unlock the mutex
                cmd_data_avail = TRUE; // set the command data available flag
            }
            else {
                syslog(LOG_ERR, "Received %zd bytes, expected %d bytes", nread, CMD_SIZE);
            }
        }
    }
    pthread_exit(NULL); // Return NULL to indicate thread completion
}



/**
 * @brief Initialize the SPI device and wiringPi library
 * @param port Port number for UDP server
 * @return uint8_t 0 on success, 1 on failure
 */
int init(char *port){

    memset(cmd_data.bytes, 0, SPI_BUF_SIZE); // clear the SPI buffer

    // Initialize the wiringPi library
    if (wiringPiSetup() == -1) {
        fprintf(stderr, "Failed to initialize wiringPi: %s\n", strerror(errno));
        return 1;
    }

    // Initialize the SPI bus
    if ((spi_fd = wiringPiSPIxSetupMode (SPI_DEV, SPI_CHAN, SPEED*MHZ,SPI_MODE_0)) < 0){
        fprintf (stderr, "Failed to open the SPI bus: %s\n", strerror (errno)) ;
        return 1;
    }
    // Initialize the UDP server
    udp_fd = init_UDP(port);
    if (udp_fd == -1) {
        fprintf(stderr, "Failed to initialize UDP server \n");
        return 1;
    }

    return 0;
}

/**
 * @brief Compute and append the crc value to the command data
 * @return uint16_t crc value
 */
uint16_t append_crc(void){
    uint8_t i = {0}; // loop index
    crc = init_val; // set crc to the initial value
    // compute crc and append to cmd_data.values
    for(i=0;i<(SPI_BUF_SIZE/2 -1);i++){
        crc = calc_crc16(crc, cmd_data.values[i],poly16);
    }
    cmd_data.values[i]=crc; // append the crc value to the command data

    return crc;
}

/**
 * @brief verify the crc value
 * @return uint16_t crc value (0 indicates success)
 */
uint16_t verify_crc(void){
    uint8_t i = {0}; // loop index
    crc = init_val; // set the initial value to crc
    // verify crc calculation
    for(i=0;i<(SPI_BUF_SIZE/2);i++){
        crc = calc_crc16(crc, cmd_data.values[i],poly16);
    }
    return crc;
}

/**
 * @brief Send the command data over SPI
 * @return 0 on success
 */
int send_SPI(void){

    pthread_mutex_lock(&mutex); // lock the data
    memcpy(TXRX_buffer, cmd_data.bytes, SPI_BUF_SIZE); // < 200 nsec latency
    pthread_mutex_unlock(&mutex); // unlock the data

    // write data over SPI, note that TXRX buffer will be overwritten with received data
    if (wiringPiSPIxDataRW (SPI_DEV,SPI_CHAN, TXRX_buffer, sizeof(TXRX_buffer)) == -1){
        syslog(LOG_ERR, "SPI failure: %s", strerror (errno)) ;
        return -1;
    } 
    return 0;
}

/********************** main ******************************/

int main (int argc, char *argv[])
{
    char* port = NULL; // port number
    pthread_t udp_thread; // thread for UDP server

    // check command line argument
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port> \n", argv[0]);
        return 1;
    } else{
        port = argv[1]; // port number
        printf("Starting KASM node on port %s\n", port);
    }

    // Initialize SPI, UDP, and wiringPi
    if(init(port) != 0){
        fprintf(stderr, "Failed initialization, exiting...\n");
        return 1;
    }

    openlog(NULL, LOG_PERROR, LOG_LOCAL6); // Open syslog for logging
    int mask = LOG_MASK(LOG_INFO) | LOG_MASK(LOG_ERR) | LOG_MASK(LOG_NOTICE);

    setlogmask(mask);
    syslog(LOG_INFO, "Starting knode on port %s.\n",port);

    pthread_mutex_init(&mutex, NULL); // initialize the mutex
    pthread_create(&udp_thread, NULL, recv_UDP, NULL); // create the UDP thread


    while(main_run == TRUE){
        if(cmd_data_avail == TRUE){
            send_SPI(); // send command over SPI to KASM PCB
            cmd_data_avail = FALSE;
        }
    }
    return 0;

}