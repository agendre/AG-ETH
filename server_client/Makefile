MCU=atmega328p
DUDECPUTYPE=m328p
LOADARG=-p $(DUDECPUTYPE) -c avrisp2 -e -U flash:w:
LOADCMD=avrdude -P usb
CC=avr-gcc
OBJCOPY=avr-objcopy
CFLAGS=-g -mmcu=$(MCU) -Wall -Wstrict-prototypes -Os -mcall-prologues 

#-------------------
.PHONY: all 
#
all: server_client.hex
	@echo "done"
#------------------
server_client.hex: server_client.elf 
	$(OBJCOPY) -R .eeprom -O ihex server_client.elf server_client.hex 
	avr-size server_client.elf
	@echo " "
	@echo "Expl.: data=initialized data, bss=uninitialized data, text=code"
	@echo " "
server_client.elf: server_client.o ip_arp_udp_tcp.o enc28j60.o websrv_help_functions.o dnslkup.o
	$(CC) $(CFLAGS) -o server_client.elf -Wl,-Map,server_client.map server_client.o ip_arp_udp_tcp.o enc28j60.o websrv_help_functions.o dnslkup.o
#
server_client.o: server_client.c ../ip_arp_udp_tcp.h ../net.h ../enc28j60.h ip_config.h ../dnslkup.h
	$(CC) $(CFLAGS) -I. -Os -c server_client.c
#------------------
websrv_help_functions.o: ../websrv_help_functions.c ../websrv_help_functions.h ip_config.h 
	$(CC) $(CFLAGS) -I. -Os -c ../websrv_help_functions.c
dnslkup.o: ../dnslkup.c  ../dnslkup.h
	$(CC) $(CFLAGS) -I. -Os -c ../dnslkup.c
enc28j60.o: ../enc28j60.c ../timeout.h ../enc28j60.h
	$(CC) $(CFLAGS) -I. -Os -c ../enc28j60.c
ip_arp_udp_tcp.o: ../ip_arp_udp_tcp.c ../net.h ../enc28j60.h ip_config.h
	$(CC) $(CFLAGS) -I. -Os -c ../ip_arp_udp_tcp.c
#------------------
load: server_client.hex
	$(LOADCMD) $(LOADARG)test_web_client.hex
#-------------------
rdfuses:
	$(LOADCMD) -p $(DUDECPUTYPE) -c avrisp2 -v -q
fuse:
	@echo "Setting standard fuses"
	$(LOADCMD) -p  $(DUDECPUTYPE) -c avrisp2 -u -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0x05:m
#-------------------
clean:
	rm -f *.o *.map *.elf *.hex
#-------------------
