#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
// http://www.nongnu.org/avr-libc/changes-1.8.html:
#define __PROG_TYPES_COMPAT__
#include <avr/pgmspace.h>
#include "../ip_arp_udp_tcp.h"
#include "../websrv_help_functions.h"
#include "../enc28j60.h"
#include "../timeout.h"
#include "../net.h"
#include "../dnslkup.h"

static uint8_t mymac[6] = {0x54,0x55,0x58,0x10,0x00,0x29};
static uint8_t myip[4] = {192,168,0,50};

#define WEBSERVER_VHOST "gendreworld.ca"
#define MYWWWPORT 80

// Default gateway. It can be set to the same as the web server in the case where the server is within the LAN
static uint8_t gwip[4] = {192,168,0,1};

// --- there should not be any need to changes things below this line ---
#define TRANS_NUM_GWMAC 1
static uint8_t gwmac[6]; 
static uint8_t otherside_www_ip[4]; // will be filled by dnslkup
//
static char urlvarstr[21];

#define BUFFER_SIZE 650
static uint8_t buf[BUFFER_SIZE+1];

#define TEMP_STRING_SIZE 20
static char gStrbuf[TEMP_STRING_SIZE];

static uint8_t start_web_client=0;
static uint8_t web_client_attempts=0;
static volatile uint8_t sec=0;
static volatile uint8_t cnt2step=0;
static int8_t dns_state=0;
static int8_t gw_arp_state=0;
static uint8_t alarm = 0;

#define LED_PIN PB5
#define LED_SETUP DDRB |= (1 << LED_PIN)
#define LEDON PORTB |= (1 << LED_PIN)
#define LEDOFF PORTB &= ~(1 << LED_PIN)

uint16_t http200ok(void)
{
        return(fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nPragma: no-cache\r\n\r\n")));
}

uint16_t fill_tcp_data_int(uint8_t *buf,uint16_t plen,int16_t i)
{
        itoa(i,gStrbuf,10); // convert integer to string
        return(fill_tcp_data(buf,plen,gStrbuf));
}

// prepare the webpage by writing the data to the tcp send buffer
uint16_t print_webpage(uint8_t *buf)
{
	uint16_t plen;
	plen = http200ok();
	plen = fill_tcp_data_p(buf,plen,PSTR("<h3>LEAK DETECTOR CURRENT STATUS</h3>"));
	plen = fill_tcp_data_p(buf,plen,PSTR("<pre>"));
	plen = fill_tcp_data_p(buf,plen,PSTR("============================\r\n"));
	plen = fill_tcp_data_p(buf,plen,PSTR("SENSOR 1: "));
	if (alarm == 1) plen = fill_tcp_data_p(buf,plen,PSTR("LEAK DETECTED!!\r\n"));
	else plen = fill_tcp_data_p(buf,plen,PSTR("GOOD\r\n"));
	plen = fill_tcp_data_p(buf,plen,PSTR("SENSOR 2: GOOD\r\n"));
	plen = fill_tcp_data_p(buf,plen,PSTR("MAINS VALVE: OPEN\r\n"));	
	plen = fill_tcp_data_p(buf,plen,PSTR("Number of Alerts sent: "));		
	plen = fill_tcp_data_int(buf,plen,web_client_attempts);
	plen = fill_tcp_data_p(buf,plen,PSTR("</pre><br>"));
	plen = fill_tcp_data_p(buf,plen,PSTR("<a href='reset'>RESET</a>"));
	return(plen);
}

void browserresult_callback(uint16_t webstatuscode,uint16_t datapos __attribute__((unused)), uint16_t len __attribute__((unused))){
	LEDOFF;
}

/* setup timer T2 as an interrupt generating time base.
* You must call once sei() in the main program */
void init_cnt2(void)
{
	cnt2step=0;
	PRR&=~(1<<PRTIM2); // write power reduction register to zero
	TIMSK2=(1<<OCIE2A); // compare match on OCR2A
	TCNT2=0;  // init counter
	OCR2A=244; // value to compare against
	TCCR2A=(1<<WGM21); // do not change any output pin, clear at compare match
	// divide clock by 1024: 12.5MHz/128=12207 Hz
	TCCR2B=(1<<CS22)|(1<<CS21)|(1<<CS20); // clock divider, start counter
	// 12207/244=50Hz
}

// timer interrupt, called automatically every second
ISR(TIMER1_COMPA_vect){
        sec++;

		if (sec>60){
			sec=0;
		}
		if ( !(PINC & (1 << PC5)) ){
			if(alarm == 0) start_web_client = 1;
			alarm = 1;
		}
}

void timer_init(void)
{
        TCNT1H=0;
        TCNT1L=0;
        TCCR1A=(0<<COM1B1)|(0<<COM1B0)|(0<<WGM11);
        TCCR1B=(1<<CS12)|(1<<CS10)|(1<<WGM12)|(0<<WGM13); // crystal clock/1024
        OCR1AH=0x3D; //16MHz: 15625=0x3D and 0x09 to get ~1second
        OCR1AL=0x09;
        TIMSK1 = (1 << OCIE1A); // interrupt mask bit
}

// the __attribute__((unused)) is a gcc compiler directive to avoid warnings about unsed variables.
void arpresolver_result_callback(uint8_t *ip __attribute__((unused)),uint8_t transaction_number,uint8_t *mac){
        uint8_t i=0;
        if (transaction_number==TRANS_NUM_GWMAC){
                // copy mac address over:
                while(i<6){gwmac[i]=mac[i];i++;}
        }
}

//Initialize the ADC for readings
void adc_init(void) {
	ADMUX = 0; //set ADC for external reference (3.3V)
	ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0); // ADC enable with clock prescale 1/128
	ADCSRA |= (1<<ADSC);  //Fire conversion just to warm ADC up
}

//Read a value from the ADC
uint16_t adc_read(void) {
	while(ADCSRA & (1<<ADSC)) {
		//Do nothing while waiting for bit to be cleared
	}
	//Bit now must be cleared so we have result
	uint16_t result = ADCL;  //Read low byte
	uint16_t temp = ADCH;  //Read high byte
	result = result + (temp<<8);  //Combine two 8bit bytes to a 16 bit integer
	
	ADCSRA |= (1<<ADSC); //set ADSC bit to start next conversion
	return(result);
}

int main(void){

        
	uint16_t dat_p,plen;

    CLKPR=(1<<CLKPCE); // change enable
    CLKPR=0; // "no pre-scaler"
    _delay_loop_1(0); // 60us

    /*initialize enc28j60*/
    enc28j60Init(mymac);
    _delay_loop_1(0); // 60us
    enc28j60PhyWrite(PHLCON,0x476);

    timer_init();
    sei();

	//Initiate the ADC
	adc_init();
		
	//Setup PD0 as output
	LED_SETUP;
	LEDON;
	
	//Setup an input on PC5
	DDRC &= ~(1 << PC5);
	PORTC |= (1 << PC5);
        
        //init the web server ethernet/ip layer:
        init_udp_or_www_server(mymac,myip);
        www_server_port(MYWWWPORT);

        while(1){
                // handle ping and wait for a tcp packet
                plen=enc28j60PacketReceive(BUFFER_SIZE, buf);
                dat_p=packetloop_arp_icmp_tcp(buf,plen);
                if(plen==0){
                        // we are idle here trigger arp and dns stuff here
                        if (gw_arp_state==0){
                                // find the mac address of the gateway (e.g your dsl router).
                                get_mac_with_arp(gwip,TRANS_NUM_GWMAC,&arpresolver_result_callback);
                                gw_arp_state=1;
                        }
                        if (get_mac_with_arp_wait()==0 && gw_arp_state==1){
                                // done we have the mac address of the GW
                                gw_arp_state=2;
				LEDOFF;
                        }
                        if (dns_state==0 && gw_arp_state==2){
                                if (!enc28j60linkup()) continue; // only for dnslkup_request we have to check if the link is up. 
                                sec=0;
                                dns_state=1;
                                dnslkup_request(buf,WEBSERVER_VHOST,gwmac);
                                continue;
                        }
                        if (dns_state==1 && dnslkup_haveanswer()){
                                dns_state=2;
                                dnslkup_get_ip(otherside_www_ip);
                        }
                        if (dns_state!=2){
                                // retry every 50s if dns-lookup failed:
                                if (sec > 50){
                                        dns_state=0;
                                }
                                // don't try to use web client before
                                // we have a result of dns-lookup
                                continue;
                        }
                        //----------
                        if (start_web_client==1){
                                LEDON;
                                start_web_client=0;
                                web_client_attempts++;
								alarm = 1;
								itoa(alarm,urlvarstr,10);
                                client_browse_url(PSTR("/leak.php?alarm="),urlvarstr,PSTR(WEBSERVER_VHOST),&browserresult_callback,otherside_www_ip,gwmac);
                        }
                        continue;
                }
                if(dat_p==0){ // plen!=0
                        // check for incomming messages not processed
                        // as part of packetloop_arp_icmp_tcp, e.g udp messages
                        udp_client_check_for_dns_answer(buf,plen);
                        continue;
                }
				if (strncmp("GET ",(char *)&(buf[dat_p]),4) != 0){
					//head, post and other methods
					dat_p = http200ok();
					dat_p = fill_tcp_data_p(buf,dat_p,PSTR("<h1>200 OK</h1>"));
					www_server_reply(buf,dat_p);
				}

				if (strncmp("/ ", (char *)&(buf[dat_p+4]),2) == 0){
					dat_p = print_webpage(buf);
					www_server_reply(buf,dat_p);
				}
				else if (strncmp("/reset ", (char *)&(buf[dat_p+4]),7) == 0){
					dat_p = http200ok();
					dat_p = fill_tcp_data_p(buf,dat_p,PSTR("<h1>Leak alarm has been reset</h1>"));
					dat_p = fill_tcp_data_p(buf,dat_p,PSTR("<a href='../'>RETURN</a>"));
					www_server_reply(buf,dat_p);
					alarm = 0;
				}
				else{
					dat_p = fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 401 Unauthorized\r\nContent-Type:text/html\r\n\r\n<h1>401 Unauthorized</h1>"));
					www_server_reply(buf,dat_p);
				}
        }
        return (0);
}
