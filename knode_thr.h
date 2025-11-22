#ifndef KNODE_THR_H
#define KNODE_THR_H

#include<stdint.h>



/**
 * @file knode.h
 * @brief Header file for knode.c
 * @author Aaron Hunter
 * @date 2025-04-28
 * @details This file contains the function declarations and global variables for the knode project.
 */

/**
 * @brief Normalize timer to account for seconds rollover
 * @param timespec_t ts Pointer to timespec structure to normalize
 */
static void normalize_timespec(struct timespec *ts);

/**
* @brief get thread info
*/
 static void getinfo();


 /**
* brief SPI write thread
* Note: This thread sleeps until a condiiton variable is signaled
* by the rec_UDP thread indicating new data is available.
* Then it sends the data over SPI to the KASM PCB.
* return: NULL
*/
void * send_SPI_thread(void *data);

/**
 * @brief Receive data over UDP thread
 * @note This thread polls for UDP packet and signals the SPI thread when new data is available. 
 * 
 */
void * recv_UDP(void *data);

/**
 * @brief Initialize the UDP server
 * @param port Port number to bind to
 * @return int Socket file descriptor   
 */
int init_UDP(char *port);

/**
 * @brief Send the command data over SPI
 * @return 0 on success
 */
int send_SPI(void);

/**
 * @brief UDP receiver thread
 * @return 
 */
void * recv_UDP(void *);

/**
 * @brief Initialize the SPI bus and wiringPi
 * 
 * @param port Port number for UDP server
 * @return uint8_t 0 on success, 1 on failure
 */
int init(char *port);

/**
 * @brief Compute and append the crc value to the command data
 * @return uint16_t crc value
 */
uint16_t append_crc(void);


/**
 * @brief Verify the crc value
 * @return uint16_t crc value (0 indicates success)
 */
uint16_t verify_crc(void);

#endif // KNODE_THR_H

