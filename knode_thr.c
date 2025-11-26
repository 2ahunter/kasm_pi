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
#include <sched.h>
#include <sys/mman.h>

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
#define REALTIME TRUE
#define NUM_THREADS 3


#define SPI_DEV0    0   
#define SPI_DEV1    1   
#define SPI_DEV3    3   
#define SPI_DEV4    4   
#define SPI_DEV5    5   
#define	SPI_CHAN	0   // only use channel 0 for all SPI devices
#define SPEED       5   // in megahertz
#define MHZ         1000000 // 1 MHz
#define CRC_INDX    26  // index of the crc value in the data structure

#define MAX_VAL     24000
#define MIN_VAL     -24000

#define PERIOD_NSEC  (400*1000) // 400 usec interval
#define NSEC_PER_SEC (1000*1000*1000)

/********** module variables *****************/
uint8_t running = TRUE; // set flag to false to terminate the threads and exit the program
uint8_t main_run = TRUE;


union CMD_DATA cmd_data[NUM_THREADS];
thread_cfg_t thread_cfgs[NUM_THREADS];

long int elapsed_time_nsec = {0}; // We only measure time < 1 sec 
int spi_fd = {0}; // file descriptor for SPI
int udp_fd = {0}; // file descriptor for UDP


// checksum parameters
uint16_t poly16 = {0x3D65}; // CRC-16-DNP polynomial
uint16_t init_val = {0xFFFF}; // initial value for CRC calculations
uint16_t crc={0}; // variable for CRC calculation

// condition variable for thread synchronization
pthread_cond_t cond_var[NUM_THREADS];

// mutexes for thread synchronization
pthread_mutex_t mutex[NUM_THREADS];
pthread_mutexattr_t mattr;

/********** functions *********************/
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

static void getinfo()
{
    struct sched_param param;
    int policy;

    sched_getparam(0, &param);
    fprintf(stderr, "Priority of this process: %d\n", param.sched_priority);

    pthread_getschedparam(pthread_self(), &policy, &param);

    fprintf(stderr, "Priority of the thread: %d, current policy is: %d and should be %d\n",
              param.sched_priority, policy, SCHED_FIFO);
}


/**
* @brief SPI write thread
* @param thr_cfg Pointer to thread configuration structure
* @note: This thread sleeps until a condiiton variable is signaled
* by the rec_UDP thread indicating new data is available.
* Then it sends the data over SPI to the KASM PCB.
* @return: NULL
*/
void * send_SPI_thread(void *thr_cfg){
    unsigned char TXRX_buffer[SPI_BUF_SIZE] = {0}; // buffer for SPI
    thread_cfg_t *cfg = (thread_cfg_t *)thr_cfg;
    memset(cmd_data[cfg->thread_id].bytes, 0, SPI_BUF_SIZE); // clear the SPI buffer

    struct timespec tmr={0};
    pthread_mutex_lock(&mutex[cfg->thread_id]);
    while(TRUE){
        // check the predicate
        while(cfg->data_ready == FALSE){
            pthread_cond_wait(&cond_var[cfg->thread_id], &mutex[cfg->thread_id]); // wait for signal
        }
        // send SPI data
        memcpy(TXRX_buffer, cmd_data[cfg->thread_id].bytes, SPI_BUF_SIZE); 
        if (wiringPiSPIxDataRW (cfg->spi_dev,cfg->spi_channel, TXRX_buffer, sizeof(TXRX_buffer)) == -1){
            syslog(LOG_ERR, "SPI failure: %s", strerror (errno)) ;
        } 
        clock_gettime(CLOCK_MONOTONIC, &tmr);
        syslog(LOG_INFO,"SPI[%d] time: %ld.%09ld",cfg->thread_id, tmr.tv_sec, tmr.tv_nsec);
        cfg->data_ready = FALSE; // reset the flag
        pthread_mutex_unlock(&mutex[cfg->thread_id]); // unlock the data
    }
    pthread_exit(NULL); // Return NULL to indicate thread completion
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
    struct timespec prd_tmr={0};
    struct timespec curr_tmr={0};
    long int delta_time_nsec = {0};
    int timeout_ms = 1000; // 1 second timeout for polling

    memset(buf_data.bytes, 0, SPI_BUF_SIZE); // init the UDP buffer

    peer_addrlen = sizeof(peer_addr);
    // set up polling
    struct pollfd fds[1]; //monitor UDP for incoming data
    fds[0].fd = udp_fd;
    fds[0].events = POLLIN; // look for new data
    int poll_ret = {0};
    getinfo();
    while(running == TRUE){
        poll_ret = poll(fds, 1, timeout_ms);
        clock_gettime(CLOCK_MONOTONIC, &prd_tmr);
        if(poll_ret < 0) exit(EXIT_FAILURE); // error
        if(poll_ret == 0) {
            syslog(LOG_WARNING, "UDP poll timeout after %d ms", timeout_ms);
        } else {
            // Receive data from the UDP socket
            nread = recvfrom(udp_fd, buf_data.bytes, CMD_SIZE, 0,
                            (struct sockaddr *)&peer_addr, &peer_addrlen);
            if (nread == -1)
            {
                syslog(LOG_ERR, "Error receiving UDP data: %s\n", strerror(errno));
            }

            if (nread == CMD_SIZE) {
                syslog(LOG_DEBUG, "Received %zd bytes", nread);
                for(int thr=0;thr<NUM_THREADS;thr++){
                    pthread_mutex_lock(&mutex[thr]); // lock the mutex
                    for (size_t i = 0; i < nread / 2; i++) {
                        cmd_data[thr].values[i] = ntohs(buf_data.values[i]);
                        syslog(LOG_DEBUG, "Received value %zu: %d\n", i, buf_data.values[i]);
                    }
                    crc = append_crc(&cmd_data[thr]); // compute the crc and append to the command data
                    thread_cfgs[thr].data_ready = TRUE; // set the data ready flag
                    pthread_cond_signal(&cond_var[thr]); // signal SPI thread that new data is available
                    pthread_mutex_unlock(&mutex[thr]); // unlock the mutex
                }
            }
            else {
                syslog(LOG_ERR, "Received %zd bytes, expected %d bytes", nread, CMD_SIZE);
            }
        }
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
        syslog(LOG_DEBUG, "Sleep until: %ld.%09ld", prd_tmr.tv_sec, prd_tmr.tv_nsec);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &prd_tmr, NULL);
    }
    pthread_exit(NULL); // Return NULL to indicate thread completion
}

/**
 * @brief Initialize the SPI devices and thread configs
 * @param port Port number for UDP server
 * @return uint8_t 0 on success, 1 on failure
 */
int init(char *port){

    // initialize the thread configurations
    for(int i=0; i< NUM_THREADS; i++){
        thread_cfgs[i].thread_id = i;
        thread_cfgs[i].spi_channel = SPI_CHAN;
        thread_cfgs[i].data_ready = FALSE;
        switch(i){
            case 0:
                thread_cfgs[i].spi_dev = SPI_DEV0;
                break;
            case 1:
                thread_cfgs[i].spi_dev = SPI_DEV1;
                break;
            case 2:
                thread_cfgs[i].spi_dev = SPI_DEV3; // we can't access SPI2 on the pi!
                break;
            case 3:
                thread_cfgs[i].spi_dev = SPI_DEV4;
                break;
            case 4:
                thread_cfgs[i].spi_dev = SPI_DEV5;
                break;
            default:
                thread_cfgs[i].spi_dev = SPI_DEV0;
                break;
        }
    }
    // Initialize the wiringPi library
    if (wiringPiSetup() == -1) {
        fprintf(stderr, "Failed to initialize wiringPi: %s\n", strerror(errno));
        return 1;
    }

    // Initialize the SPI buses
    for(int i=0; i< NUM_THREADS; i++){
        if ((spi_fd = wiringPiSPIxSetupMode (thread_cfgs[i].spi_dev, thread_cfgs[i].spi_channel, SPEED*MHZ,SPI_MODE_0)) < 0){
            fprintf (stderr, "Failed to open the SPI bus: %s\n", strerror (errno)) ;
            return 1;
        }
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
uint16_t append_crc(union CMD_DATA * data ){
    uint8_t i = {0}; // loop index
    crc = init_val; // set crc to the initial value
    // compute crc and append to data->values
    for(i=0;i<(SPI_BUF_SIZE/2 -1);i++){
        crc = calc_crc16(crc, data->values[i],poly16);
    }
    data->values[i]=crc; // append the crc value to the command data
    return crc;
}

/**
 * @brief verify the crc value
 * @return uint16_t crc value (0 indicates success)
 */
uint16_t verify_crc(union CMD_DATA * data){
    uint8_t i = {0}; // loop index
    crc = init_val; // set the initial value to crc
    // verify crc calculation
    for(i=0;i<(SPI_BUF_SIZE/2);i++){
        crc = calc_crc16(crc, data->values[i],poly16);
    }
    return crc;
}


/********************** main ******************************/

int main (int argc, char *argv[])
{
    // Lock the memory
    mlockall(MCL_CURRENT | MCL_FUTURE);


    // Set mutex to priority inheritance
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setprotocol(&mattr, PTHREAD_PRIO_INHERIT);
    // Initialize the condition variables and mutexes
    for(int i=0;i<NUM_THREADS;i++){
        // cond_var[i] = PTHREAD_COND_INITIALIZER;
        pthread_cond_init(&cond_var[i], NULL);
        pthread_mutex_init(&mutex[i], &mattr);
    }



    char* port = NULL; // port number
    pthread_t udp_thread; // thread for UDP server
    pthread_t spi_thread[NUM_THREADS]; // thread for SPI communication

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

    openlog(NULL, LOG_PID, LOG_LOCAL6); // Open syslog for logging
    int mask = LOG_MASK(LOG_INFO) | LOG_MASK(LOG_ERR) | LOG_MASK(LOG_NOTICE);
    // int mask = LOG_MASK(LOG_ERR);

    setlogmask(mask);

    //TODO convert this to an init function 
    // Initialize the pthread attributes
    pthread_attr_t attr;
    int ret = pthread_attr_init(&attr);
    if (ret != 0){
        syslog(LOG_ERR, "pthread_attr_init error: %d, meaning: %s\n", ret, strerror(ret));
        return 1;
    }

    // Set the scheduler policy
    ret = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    if (ret != 0){
        syslog(LOG_ERR, "pthread_attr_setschedpolicy error: %d, meaning: %s\n", ret, strerror(ret));
        return 1;
    }

    // Set the scheduler priority
    struct sched_param param;
    param.sched_priority = 80;
    ret = pthread_attr_setschedparam(&attr, &param);
    if (ret != 0){
        syslog(LOG_ERR, "pthread_attr_setschedparam error: %d, meaning: %s\n", ret, strerror(ret));
        return 1;
    }

    // Make sure threads created using the thread_attr_ takes the value
    // from the attribute instead of inherit from the parent thread.
    ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    if (ret != 0){
        syslog(LOG_ERR, "pthread_attr_setinheritsched error: %d, meaning: %s\n", ret, strerror(ret));
        return 1;
    }

    syslog(LOG_INFO, "Starting knode\n");
    if(REALTIME==TRUE){
        pthread_create(&udp_thread, &attr, recv_UDP, NULL); // create the UDP thread
        for(int i=0;i<NUM_THREADS;i++){
            pthread_create(&spi_thread[i],&attr, send_SPI_thread, &thread_cfgs[i]); // create the SPI threads
        }
    } else {
        pthread_create(&udp_thread, NULL, recv_UDP, NULL); // create the UDP thread
        for(int i=0;i<NUM_THREADS;i++){
            pthread_create(&spi_thread[i], NULL, send_SPI_thread, &thread_cfgs[i]); // create the SPI threads
        }
    }

    pthread_join(udp_thread, NULL);
    for(int i=0;i<NUM_THREADS;i++){
        pthread_join(spi_thread[i], NULL);
    }
    return 0;

}
