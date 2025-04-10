#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <linux/spi/spidev.h>
#include "wiringPi.h"
#include "wiringPiSPI.h"


#define	TRUE	(1==1)
#define	FALSE	(!TRUE)

#define SPI_DEV     1   // spi device number
#define	SPI_CHAN	2   // which chip select
#define SPEED       5   // in megahertz
#define BUF_SIZE    52
#define MAX_VAL     24000
#define MIN_VAL     -24000

union CMD_DATA {
    unsigned char bytes[BUF_SIZE];
    int16_t values[BUF_SIZE/2];
} cmd_data;

union CMD_DATA* cmd_data_ptr = &cmd_data;


int main (int argc, char *argv[])
{
    // Check for correct number of arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <index> <value>\n", argv[0]);
        return 1;
    }

    // Parse the command line argument
    uint8_t index = atoi(argv[1]);
    if (index < 0 || index >= BUF_SIZE) {
        fprintf(stderr, "Invalid index. Must be between 0 and %d.\n", 
            BUF_SIZE/2 - 1);
        return 1;
    }
    // Parse the command line argument
    // Check if the command is a valid integer
    int command = atoi(argv[2]);
    if (command < MIN_VAL || command > MAX_VAL) {
        fprintf(stderr, "Invalid command. Must be between -24000 and 24000.\n");
        return 1;
    }


    unsigned char TXRX_buffer[BUF_SIZE];
    int i = {0};
    int Fd = {0};
    int copy_success = TRUE;

    // populate the buffer with random values
    for(i = 0; i < BUF_SIZE/2; i++){
        int16_t value = (rand() % 0xFFFF)>>8; // generate small random value
        cmd_data.values[i] = value;
    }

    // Set the indexed value to the command
    cmd_data.values[index] = command;

    // print the values
    for(i = 0; i < BUF_SIZE/2; i++){
        printf("%d ", cmd_data.values[i]);
    }
    printf("\r\n");
   
    // Fill the buffer with the command data
    memcpy(TXRX_buffer, cmd_data_ptr, BUF_SIZE); 

    wiringPiSetup();  // init wiringpi
    
    // Init SPI port
    if ((Fd = wiringPiSPIxSetupMode (SPI_DEV, SPI_CHAN, SPEED*100000,SPI_MODE_0)) < 0){
        fprintf (stderr, "Can't open the SPI bus: %s\n", strerror (errno)) ;
        exit (EXIT_FAILURE) ;
    }

    /* SPI transaction, note that TXRX buffer will be overwritten with received data */
    if (wiringPiSPIxDataRW (SPI_DEV,SPI_CHAN, TXRX_buffer, sizeof(TXRX_buffer)) == -1){
        printf ("SPI failure: %s\n", strerror (errno)) ;
        exit (EXIT_FAILURE);
    } else {

        for(i = 0; i < BUF_SIZE; i ++){
            if(TXRX_buffer[i] != cmd_data.bytes[i]) copy_success = FALSE;
            printf("%x ", TXRX_buffer[i]);
        }
        if(copy_success == TRUE) printf("\r\nSuccess! \r\n");
        else printf("\r\ncopy failed! \r\n");
    }

    return 0;

}