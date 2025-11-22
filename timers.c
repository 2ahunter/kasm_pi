#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "timers.h"


static struct timespec start = {0};
static struct timespec end ={0};
static long int elapsed_time= {0};

/**
 * @brief Starts the timer.
 * @note Uses CLOCK_MONOTONIC to measure elapsed time. Use only for time intervals < 1 second
 */
void start_timer() {
    if(clock_gettime(CLOCK_MONOTONIC, &start)==-1){
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Stops the timer and prints the elapsed time.
 * @note Uses CLOCK_MONOTONIC to measure elapsed time. Use only for time intervals < 1 second
 */
long int stop_timer() {
    if(clock_gettime(CLOCK_MONOTONIC, &end)==-1){
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    long int sec = (end.tv_sec - start.tv_sec) * 1e9;
    elapsed_time = (end.tv_nsec - start.tv_nsec + sec) ;
    return elapsed_time;
}