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

#include "wiringPi.h"
#include "wiringPiSPI.h"
#include "crc_check.h"
#include "timers.h"


/*************** defines **********************/
#define UDP_BUF_SIZE 500
#define CMD_SIZE 52 // 52 bytes for 26 int16_t values

#define	TRUE	(1==1)
#define	FALSE	(!TRUE)

#define SPI_DEV     1   // SPI device number
#define	SPI_CHAN	2   // which chip select
#define SPEED       10   // in megahertz
#define SPI_BUF_SIZE    54 // bytes, including crc16
#define CRC_INDX    26  // index of the crc value in the data structure

#define MAX_VAL     24000
#define MIN_VAL     -24000

/********** module variables *****************/
uint8_t cmd_data_avail = FALSE; // flag to indicate if command data is available

union CMD_DATA {
    unsigned char bytes[SPI_BUF_SIZE];
    int16_t values[SPI_BUF_SIZE/2];
} cmd_data, buf_data;


int spi_fd = {0}; // file descriptor for SPI
int udp_fd = {0}; // file descriptor for UDP
unsigned char TXRX_buffer[SPI_BUF_SIZE] = {0}; // buffer for SPI

// checksum parameters
uint16_t poly16 = {0x3D65}; // CRC-16-DNP polynomial
uint16_t init_val = {0xFFFF}; // initial value for CRC calculations
uint16_t crc={0}; // variable for CRC calculation

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
    struct sockaddr_storage peer_addr;
    socklen_t peer_addrlen;

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
int recv_UDP(void){
    int s;
    ssize_t nread;
    socklen_t peer_addrlen;
    struct sockaddr_storage peer_addr;

    char host[NI_MAXHOST], service[NI_MAXSERV];

    peer_addrlen = sizeof(peer_addr);
    nread = recvfrom(udp_fd, buf_data.bytes, CMD_SIZE, 0,
                        (struct sockaddr *) &peer_addr, &peer_addrlen);
    if (nread == -1){
        return -1; // Error receiving data
    }

    s = getnameinfo((struct sockaddr *) &peer_addr,
                    peer_addrlen, host, NI_MAXHOST,
                    service, NI_MAXSERV, NI_NUMERICSERV);
    if (s == 0) {
        printf("Received %zd bytes from %s:%s\n",
                nread, host, service);
        for (size_t i = 0; i < nread/2; i++) {
            cmd_data.values[i] = ntohs(buf_data.values[i]);
            printf("Received value %zu: %d\n", i, cmd_data.values[i]);
        }
        cmd_data_avail = TRUE; // set the command data available flag
    }
    else
        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));

    if (sendto(udp_fd, buf_data.bytes, nread, 0, (struct sockaddr *) &peer_addr,
                peer_addrlen) != nread)
    {
        fprintf(stderr, "Error sending response\n");
    }
    return s;
}



/**
 * @brief Initialize the SPI device and wiringPi library
 * @param port Port number for UDP server
 * @return uint8_t 0 on success, 1 on failure
 */
int init(char *port){
    uint8_t i = {0}; // loop index
    // init SPI buffer
    memset(cmd_data.bytes, 0, SPI_BUF_SIZE); // clear the SPI buffer
    memset(buf_data.bytes, 0, SPI_BUF_SIZE); // clear the UDP buffer

    // Initialize the wiringPi library
    if (wiringPiSetup() == -1) {
        fprintf(stderr, "Failed to initialize wiringPi: %s\n", strerror(errno));
        return 1;
    }

    // Initialize the SPI bus
    if ((spi_fd = wiringPiSPIxSetupMode (SPI_DEV, SPI_CHAN, SPEED*100000,SPI_MODE_0)) < 0){
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
    
    uint8_t i = {0}; // loop index
    // Fill the buffer with the command data
    memcpy(TXRX_buffer, cmd_data.bytes, SPI_BUF_SIZE); // < 200 nsec latency
    start_timer(); // start the timer
    // write data over SPI, note that TXRX buffer will be overwritten with received data
    if (wiringPiSPIxDataRW (SPI_DEV,SPI_CHAN, TXRX_buffer, sizeof(TXRX_buffer)) == -1){
        printf ("SPI failure: %s\n", strerror (errno)) ;
        return -1;
    } else {
        printf("Data received: \n");
        for(i = 0; i < SPI_BUF_SIZE; i ++){
            printf("%x ", TXRX_buffer[i]);
        }
    }
    stop_timer(); // stop the timer
    return 0;
}

/********************** main ******************************/

int main (int argc, char *argv[])
{
    char* port = NULL; // port number
    uint32_t i = {0}; // loop index
    int res = {0}; // return value

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


    while(1){
        res = recv_UDP(); // receive data over UDP
        if(cmd_data_avail == TRUE){
            //lock the data 
            // copy the data to the command data structure
            // unlock the data
            // compute the crc and append to the command data
            crc = append_crc();
            // verify the crc value
            crc = verify_crc();
            if(crc == 0){
                printf("CRC verified\n");
                send_SPI(); // send command over SPI to KASM PCB
            } else {
                printf("CRC check failed: %x \n", crc);
            }
            cmd_data_avail = FALSE;
        }

    }

    return 0;

}