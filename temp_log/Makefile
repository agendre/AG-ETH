MCU=atmega328p
DUDECPUTYPE=m328p
LOADARG=-p $(DUDECPUTYPE) -c avrisp2 -e -U flash:w:
LOADCMD=avrdude -P usb
CC=avr-gcc
OBJCOPY=avr-objcopy
CFLAGS=-g -mmcu=$(MCU) -Wall -Wstrict-prototypes -Os -mcall-prologues 
PRJNAME=temp_log

#-------------------
.PHONY: all 
#
all: $(PRJNAME).hex
	@echo "done"
#------------------
$(PRJNAME).hex: $(PRJNAME).elf 
	$(OBJCOPY) -R .eeprom -O ihex $(PRJNAME).elf $(PRJNAME).hex 
	avr-size $(PRJNAME).elf
	@echo " "
	@echo "Expl.: data=initialized data, bss=uninitialized data, text=code"
	@echo " "
$(PRJNAME).elf: $(PRJNAME).o ip_arp_udp_tcp.o enc28j60.o websrv_help_functions.o dnslkup.o
	$(CC) $(CFLAGS) -o $(PRJNAME).elf -Wl,-Map,$(PRJNAME).map $(PRJNAME).o ip_arp_udp_tcp.o enc28j60.o websrv_help_functions.o dnslkup.o
#
$(PRJNAME).o: $(PRJNAME).c ../ip_arp_udp_tcp.h ../net.h ../enc28j60.h ip_config.h ../dnslkup.h
	$(CC) $(CFLAGS) -I. -Os -Wl,-u,vfprintf -lprintf_flt -lm -c $(PRJNAME).c
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
load: $(PRJNAME).hex
	$(LOADCMD) $(LOADARG)$(PRJNAME).hex
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
