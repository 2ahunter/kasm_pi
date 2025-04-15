#include<stdio.h>
#include<stdint.h>
#include <arpa/inet.h>
#include"crc_check.h"

#define DATASIZE 32
#define MSBMASK 0x80000000
#define CRC_TESTING 1

uint32_t poly = 0x04C11DB7; // default 32 bit CRC polynomial

union DATA32{
    uint32_t val;
    uint8_t bytes[4];
};

/**
 * @brief returns the remainder of binary division between initial value and crc polynomial
 * 
 * @param init_val value from prior crc calculation
 * @param data 
 * @param poly
 * @return uint32_t 
 */
uint32_t calc_crc32(uint32_t init_val, uint32_t data, uint32_t poly){
    uint8_t index={0};
    uint32_t crc={0};

    // initial XOR of initial value and input data
    crc = init_val ^ data;

    // CRC algorithm
    for(index=0; index < DATASIZE; index++){
        if(crc&MSBMASK) {
            crc = (crc << 1) ^ poly;
        }
        else {
            crc = (crc << 1);
        }
    }
    return crc;
}


#ifdef CRC_TESTING
int main(int argc, char **argv)
{
    union DATA32 data1, data2;
    uint8_t index={0};
    data1.val = 0x08070605;
    data2.val = 0x05060708;
    uint32_t data3 = {0x12345678};

    printf("%08X\n", calc_crc32(0xFFFFFFFF, data1.val, poly)); // 0xC9F6D629
    for(index=0;index<4;index++){
        printf("%02X,", data1.bytes[index]);
    }
    printf("\n");

    data1.val = htonl(data1.val); // convert host data to network byte order
    printf("%08X\n", calc_crc32(0xFFFFFFFF, data1.val, poly)); // 0x72887319
    for(index=0;index<4;index++){
        printf("%02X,", data1.bytes[index]);
    }
    printf("\n");

    printf("%08X\n", calc_crc32(0xFFFFFFFF, data2.val,poly)); // 0x72887319
    printf("%08X\n", calc_crc32(0xFFFFFFFF, data3,poly));  // 0XDF8A8A2B

    return(1);
}
#endif


