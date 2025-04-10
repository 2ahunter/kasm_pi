# kasm_pi
Raspberry Pi interface to KASM controller PCB

# Note 
SPI1 interface (with three channels) and UART3 (along with serial console) are enabled.

The /boot/firmware/config.txt includes the following lines:

dtoverlay=spi1-3cs

dtoverlay=uart3
