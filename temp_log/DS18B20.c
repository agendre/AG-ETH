#include <avr/io.h> // deal with port registers 
#include <util/delay.h> // used for _delay_us function 
#include <stdlib.h> 
#include <stdio.h>


#define F_CPU 16000000L // run CPU at 16 MHz 
#define ClearBit(x,y) x &= ~_BV(y) // equivalent to cbi(x,y) 
#define SetBit(x,y) x |= _BV(y) // equivalent to sbi(x,y) 
#define ReadBit(x,y) x & _BV(y) // call with PINx and bit# 

typedef uint8_t byte; // I just like byte & sbyte better 
typedef int8_t sbyte;

void InitAVR() 
{ 
 DDRC = 0x00; // 0000.0000; set PORTC as inputs 
} 

void msDelay(int delay) // put into a routine to remove code inlining at cost of timing accuracy
{
 for (int i=0;i<delay;i++)
 _delay_ms(1);
} 
 
 // ----------"ONE-WIRE" ROUTINES ----------------------
#define THERM_PORT PORTC 
#define THERM_DDR DDRC 
#define THERM_PIN PINC 
#define THERM_IO 0 
 
#define THERM_INPUT() ClearBit(THERM_DDR,THERM_IO) // make pin an input 
#define THERM_OUTPUT() SetBit(THERM_DDR,THERM_IO) // make pin an output 
#define THERM_LOW() ClearBit(THERM_PORT,THERM_IO) // take (output) pin low 
#define THERM_HIGH() SetBit(THERM_PORT,THERM_IO) // take (output) pin high 
#define THERM_READ() ReadBit(THERM_PIN,THERM_IO) // get (input) pin value 
 
#define THERM_CONVERTTEMP 0x44 
#define THERM_READSCRATCH 0xBE 
#define THERM_WRITESCRATCH 0x4E 
#define THERM_COPYSCRATCH 0x48 
#define THERM_READPOWER 0xB4 
#define THERM_SEARCHROM 0xF0 
#define THERM_READROM 0x33 
#define THERM_MATCHROM 0x55 
#define THERM_SKIPROM 0xCC 
#define THERM_ALARMSEARCH 0xEC 

// the following arrays specify the addresses of *my* ds18b20 devices 
// substitute the address of your devices before using. 
byte rom0[] = {0x28,0xE1,0x21,0xA3,0x02,0x00,0x00,0x5B}; 
byte rom1[] = {0x28,0x1B,0x21,0x30,0x05,0x00,0x00,0xF5}; 
 
byte therm_Reset() 
{ 
  byte i; 
  THERM_OUTPUT(); // set pin as output 
  THERM_LOW(); // pull pin low for 480uS 
  _delay_us(480); 
  THERM_INPUT(); // set pin as input 
  _delay_us(60); // wait for 60uS 
  i = THERM_READ(); // get pin value 
  _delay_us(420); // wait for rest of 480uS period 
 return i; 
} 

void therm_WriteBit(byte bit) 
{ 
  THERM_OUTPUT(); // set pin as output 
  THERM_LOW(); // pull pin low for 1uS 
  _delay_us(1); 
  if (bit) THERM_INPUT(); // to write 1, float pin 
  _delay_us(60); 
  THERM_INPUT(); // wait 60uS & release pin 
}

byte therm_ReadBit() 
{ 
 byte bit=0; 
 THERM_OUTPUT(); // set pin as output 
 THERM_LOW(); // pull pin low for 1uS 
 _delay_us(1); 
 THERM_INPUT(); // release pin & wait 14 uS 
 _delay_us(14); 
 if (THERM_READ()) bit=1; // read pin value 
 _delay_us(45); // wait rest of 60uS period 
 return bit; 
} 

void therm_WriteByte(byte data) 
{ 
 byte i=8; 
 while(i--) // for 8 bits: 
 { 
 therm_WriteBit(data&1); // send least significant bit 
 data >>= 1; // shift all bits right 
 } 
} 
 
byte therm_ReadByte() 
{ 
 byte i=8, data=0; 
 while(i--) // for 8 bits: 
 { 
 data >>= 1; // shift all bits right 
 data |= (therm_ReadBit()<<7); // get next bit (LSB first) 
 } 
 return data; 
} 

void therm_MatchRom(byte rom[]) 
{ 
 therm_WriteByte(THERM_MATCHROM); 
 for (byte i=0;i<8;i++) 
 therm_WriteByte(rom[i]); 
} 

void therm_ReadTempRaw(byte id[], byte *t0, byte *t1) 
// Returns the two temperature bytes from the scratchpad 
{ 
 therm_Reset(); // skip ROM & start temp conversion 
 if (id) therm_MatchRom(id); 
 else therm_WriteByte(THERM_SKIPROM); 
 therm_WriteByte(THERM_CONVERTTEMP); 
 while (!therm_ReadBit()); // wait until conversion completed 
 
 therm_Reset(); // read first two bytes from scratchpad 
 if (id) therm_MatchRom(id); 
 else therm_WriteByte(THERM_SKIPROM); 
 therm_WriteByte(THERM_READSCRATCH); 
 *t0 = therm_ReadByte(); // first byte 
 *t1 = therm_ReadByte(); // second byte 
}

void therm_ReadTempC(byte id[], int *whole, int *decimal) 
// returns temperature in Celsius as WW.DDDD, where W=whole & D=decimal 
{ 
 byte t0,t1; 
 therm_ReadTempRaw(id,&t0,&t1); // get temperature values 
 *whole = (t1 & 0x07) << 4; // grab lower 3 bits of t1 
 *whole |= t0 >> 4; // and upper 4 bits of t0 
 *decimal = t0 & 0x0F; // decimals in lower 4 bits of t0 
  *decimal *= 625; // conversion factor for 12-bit resolution 
}