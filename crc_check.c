#include<stdio.h>
#include<stdint.h>
#include <arpa/inet.h>
#include"crc_check.h"

#define D32_SZ 32
#define D16_SZ 16
#define MSBMASK_32 0x80000000
#define MSBMASK_16 0x8000
// #define CRC_TESTING 1


union DATA32{
    uint32_t val;
    uint8_t bytes[4];
};

union DATA16{
    uint16_t val;
    uint8_t bytes[2];
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
    for(index=0; index < D32_SZ; index++){
        if(crc&MSBMASK_32) {
            crc = (crc << 1) ^ poly;
        }
        else {
            crc = (crc << 1);
        }
    }
    return crc;
}

/**
 * @brief returns the remainder of binary division between initial value and crc polynomial
 * 
 * @param init_val value from prior crc calculation
 * @param data 
 * @param poly
 * @return uint16_t 
 */
uint16_t calc_crc16(uint16_t init_val, uint16_t data, uint16_t poly){
    uint8_t index={0};
    uint16_t crc = {0}; 
    
    // initial XOR of initial value and input data
    crc = init_val ^ data;

    // CRC algorithm
    for(index=0; index < D16_SZ; index++){
        if(crc&MSBMASK_16) {
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
    uint32_t poly32 = 0x04C11DB7; // default 32 bit CRC polynomial
    // CRC-16-DNP
    uint16_t poly16 = 0x3D65; // => x^{16}+x^{13}+x^{12}+x^{11}+x^{10}+x^{8}+x^{6}+x^{5}+x^{2}+1
    union DATA32 data1, data2;
    uint8_t index={0};
    data1.val = 0x08070605;
    data2.val = 0x05060708;
    uint32_t data3 = {0x12345678};

    union DATA16 data4,data5;
    data4.val = 0xdead;
    data5.val = 0xbeef;


    printf("%08X\n", calc_crc32(0xFFFFFFFF, data1.val, poly32)); // 0xC9F6D629
    for(index=0;index<4;index++){
        printf("%02X,", data1.bytes[index]);
    }
    printf("\n");

    data1.val = htonl(data1.val); // convert host data to network byte order
    printf("%08X\n", calc_crc32(0xFFFFFFFF, data1.val, poly32)); // 0x72887319
    for(index=0;index<4;index++){
        printf("%02X,", data1.bytes[index]);
    }
    printf("\n");

    printf("%08X\n", calc_crc32(0xFFFFFFFF, data2.val,poly32)); // 0x72887319
    printf("%08X\n", calc_crc32(0xFFFFFFFF, data3,poly32));  // 0XDF8A8A2B

    printf("%04X\n", calc_crc16(0xFFFF,data4.val,poly16)); // 0X7137
    printf("%04X\n", calc_crc16(0xFFFF,data5.val,poly16)); // 0XC2FF


    return(1);
}
#endif


