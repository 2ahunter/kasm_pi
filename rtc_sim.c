/**
*   rtc_sim.c
*   Simulated RTC UDP packets sent to KASM Pi node for latency testing
*   Author: Aaron Hunter
*   Date: 2025-11-19
*
**/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <sched.h>
#include <sys/mman.h> // necessary for mlockall
#include "UDP_client.h"

#define PERIOD_NSEC  (1000*1000) // 500 usec interval
#define NSEC_PER_SEC (1000*1000*1000)
#define TRUE (1==1)
#define FALSE (!TRUE)
#define REALTIME TRUE


static void normalize_timespec(struct timespec *ts) {
    while (ts->tv_nsec >= NSEC_PER_SEC) {
        ts->tv_sec += 1;
        ts->tv_nsec -= NSEC_PER_SEC;
    }
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


void *rtc_sim_thread(void *arg){
    union CMD_DATA cmd_data;
    struct timespec prd_tmr={0};

    getinfo(); 
    clock_gettime(CLOCK_MONOTONIC, &prd_tmr);
    
    while (1) {
        // populate the buffer with random values
        for(int i = 0; i < CMD_SIZE/2; i++){
            int16_t value = (rand() % 0xFFFF)>>8; // generate small random values
            cmd_data.values[i] = htons(value);
        }
        // Send RTC data over UDP
        int sent = UDP_send(cmd_data);
        if (sent == 0){
            fprintf(stderr, "RTC sim: Failed to send UDP packet\n");
        }
        // Calculate next wake-up time
        prd_tmr.tv_nsec += PERIOD_NSEC;
        normalize_timespec(&prd_tmr);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &prd_tmr, NULL);
    }
    return NULL;
}



/********************** main ******************************/

int main (int argc, char *argv[])
{
    char ip_addr[] = "127.0.0.1"; // IP address
    char port[] = "2345"; // port number
    int res = {0}; // return value

    // Lock the memory
    if (REALTIME==TRUE) {
        res = mlockall(MCL_CURRENT|MCL_FUTURE);
    }

    // check command line arguments
    // if (argc != 3) {
    //     fprintf(stderr, "Usage: %s <host> <port> \n", argv[0]);
    //     return 1;
    // } else{
    //     *ip_addr = argv[1]; 
    //     *port = argv[2];
    // }

    // Initialize UDP
    printf("Starting RTC simulation on port %s:%s\n", ip_addr, port);
    res = UDP_init(ip_addr, port);
    if (res < 0){
        fprintf(stderr, "Failed UDP initialization, exiting...\n",res);
        return 1;
    } else {
        printf("UDP client initalized\n");
    }

    // Initialize the pthread attributes
    pthread_attr_t attr;
    int ret = pthread_attr_init(&attr);
    if (ret != 0){
        fprintf(stderr, "Failed to initialize pthread attributes\n");
        fprintf(stderr, "due to error: %d, meaning: %s\n", ret, strerror(ret));
        return 1;
    }

    // Set the scheduler policy
    ret = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    if (ret != 0){
        fprintf(stderr, "Failed to set scheduler policy\n");
        fprintf(stderr, "due to error: %d, meaning: %s\n", ret, strerror(ret));
        return 1;
    }

    // Set the scheduler priority
    struct sched_param param;
    param.sched_priority = 81;
    ret = pthread_attr_setschedparam(&attr, &param);
    if (ret != 0){
        fprintf(stderr, "Failed to set scheduler parameters,\n");
        fprintf(stderr, "due to error: %d, meaning: %s\n", ret, strerror(ret));
        return 1;
    }

    // Make sure threads created using the thread_attr_ takes the value
    // from the attribute instead of inherit from the parent thread.
    ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    if (ret != 0){
        fprintf(stderr, "Failed to set explicit scheduling\n");
        fprintf(stderr, "due to error: %d, meaning: %s\n", ret, strerror(ret));
        return 1;
    }

    // Finally, create RTC simulation thread
    pthread_t rtc_thread;
    if (REALTIME == TRUE){
        ret = pthread_create(&rtc_thread, &attr, rtc_sim_thread, NULL);
    } else {
        ret = pthread_create(&rtc_thread, NULL, rtc_sim_thread, NULL);
    }
    if (ret != 0){
        fprintf(stderr, "Failed to create thread due to error: %d, meaning: %s\n", ret, strerror(ret));
        return 1;
    }
    pthread_join(rtc_thread, NULL);
}

    
