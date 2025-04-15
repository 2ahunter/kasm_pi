#ifndef CRC_CHECK_H_
#define CRC_CHECK_H_


#include<stdint.h>

/**
 * @brief returns the remainder of binary division between initial value and crc polynomial
 * 
 * @param init_val value from prior crc calculation
 * @param data 
 * @param poly
 * @return uint32_t 
 */
uint32_t calc_crc32(uint32_t init_val, uint32_t data, uint32_t poly);

#endif //CRC_CHECK_H_