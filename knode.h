/**
 * @file knode.h
 * @brief Header file for knode.c
 * @author Aaron Hunter
 * @date 2025-04-28
 * @details This file contains the function declarations and global variables for the knode project.
 */

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

