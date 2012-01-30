/*
 * 
 * 
 * power monitor v2 firmware
 * dan bedard, RENCI
 * 
 * 2: fixed watchdog timer continuous reset by adding MCUSR = 0;
 */

// (c) 2008, Renci
// Available under the Renci Open Source Software License v1.0

#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <stdint.h>
#include "usart.h"
#include "timer2.h"
#include "adm1191.h"

#define OVF_FLAG 31
#define TIME_FLAG 30
#define DONE_FLAG 29
#define READING_BUFFER_LENGTH 12
#define READING_BUFFER_END READING_BUFFER_LENGTH - 1
#define HW_VERSION_H 2
#define HW_VERSION_L 0
#define SW_VERSION_H 2
#define SW_VERSION_L 2
#define WD_TIMEOUT WDTO_2S

FILE uart_str = FDEV_SETUP_STREAM((int(*)(char, FILE *))USART_Transmit, (int(*)(FILE *))USART_Receive, _FDEV_SETUP_RW);

static uint16_t sensor_mask;
static uint32_t reading_buffer[READING_BUFFER_LENGTH];
static volatile uint16_t reading_head_ptr;
static volatile uint16_t reading_tail_ptr;
static volatile uint8_t overflow;

void initialize_sensors() {
	uint8_t bytes[3];
	uint8_t sensor_cfg = 0x05;
	uint8_t i, sensor;
	for (i = 0; i < 16; i++) {
		if (sensor_mask & (1 << i)) {
			sensor = 0x60 | (i << 1);
			adm1191_put(&sensor_cfg, 1, sensor);
			adm1191_get(bytes, sizeof(bytes), sensor); /* get and throw away */
		}
	}
}

void init_reading_buffer(void) {
	reading_head_ptr = 0;
	reading_tail_ptr = 0;
	overflow = 0;
}

void put_reading(uint32_t reading) { /* tags overflow! */	
	reading_tail_ptr = (reading_tail_ptr == READING_BUFFER_END) ? 0
			: reading_tail_ptr + 1;
	if(reading_tail_ptr == reading_head_ptr) {
		overflow = 1;
	}
	reading_buffer[reading_tail_ptr] = overflow ? (reading
			| ((uint32_t)1 << OVF_FLAG)) : reading;	 
}

uint32_t get_reading(void) {
	if (reading_head_ptr == reading_tail_ptr) {
		return 0;
	}	
	reading_head_ptr = (reading_head_ptr == READING_BUFFER_END) ? 0
			: reading_head_ptr + 1;
	return reading_buffer[reading_head_ptr];
}

uint16_t readings_available(void) {
	if (reading_head_ptr <= reading_tail_ptr) {
		return (reading_tail_ptr - reading_head_ptr);
	} else {
		return (READING_BUFFER_LENGTH + reading_tail_ptr - reading_head_ptr);
	}
}

static void trigger_function(void) {
	uint8_t bytes[3];
	uint8_t i;	

	for (i = 0; i < 16; i++) {
		if (sensor_mask & (1 << i)) {
			if (adm1191_get(bytes, sizeof(bytes), (0x60 | (i << 1))) == 3) {
				put_reading(((uint32_t)i << 24) | ((uint32_t)bytes[0] << 16) |
						  ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2]));				
			} else {
				put_reading((uint32_t)i<< 24);
			}
		}
	}
}

static void timestamp_function(uint32_t time) {	
	put_reading(time | ((uint32_t)1 << TIME_FLAG));
}

uint8_t get_command(void) {
	char buf[128];
	char c;
	uint16_t arg1;
	uint32_t arg2;
	arg1 = 0;
	arg2 = 0;

	wdt_disable();
	printf("OK\r\n");

	if (fgets(buf, sizeof(buf), stdin)==NULL) {
		return 1; /* reinitialize */
	}

	sscanf(buf, "%c %u %lu", &c, &arg1, &arg2);
	
	switch (c) {
	
	case 'v':
		printf("PowerMon\r\n HW v%d.%d\r\n SW v%d.%d\r\n", HW_VERSION_H, HW_VERSION_L, SW_VERSION_H, SW_VERSION_L);
		return 1;		

	case 't':
		if (arg1) {
			timer2_set_time(arg1);
		}
		printf("T=%lu\r\n", timer2_get_time());
		return 1;

	case 's':
		timer2_set_trigger(arg1, arg2);
		printf("S=%u,%lu\r\n", arg1, arg2);
		return 1;

	case 'm':
		sensor_mask = arg1;
		printf("M=%u\r\n", arg1);
		return 1;

    case 'P':
        wdt_enable(WDTO_15MS);
        while(1);
        return 0;        

	case 'e':
		return 2;

	default:
		return 1;
	}
}

uint8_t check_for_done(void) {
	uint32_t reading;
	if (readings_available()) {
		reading = get_reading();
		USART_Transmit((uint8_t)(reading>>24));
		USART_Transmit((uint8_t)(reading>>16));
		USART_Transmit((uint8_t)(reading>>8));
		USART_Transmit((uint8_t)reading);
/*		if(reading & (uint32_t)1<<OVF_FLAG) {
			printf("*");
		}
		if(reading & (uint32_t)1<<TIME_FLAG) {
			printf("%lu\r\n", reading & ~((uint32_t)1<<TIME_FLAG));
	} else {*/
/*			uint8_t sensor = reading >> 24 & 0x0F;
			uint16_t voltage = ((reading >> 12) & 0xFF0) | ((reading >> 4) & 0x0F);
			uint16_t current = ((reading >> 4) & 0xFF0) | (reading & 0x0F);*/

	}
	if (timer2_get_trigger()) {
		while (DataInReceiveBuffer()) {
			char c= getchar();
			if (c == 'd') {
				timer2_trigger_disable();
			}
		}
	}
	if (!readings_available() && !timer2_get_trigger()) {
		reading = ((uint32_t)1 << DONE_FLAG);
		USART_Transmit((uint8_t)(reading>>24));
		USART_Transmit((uint8_t)(reading>>16));
		USART_Transmit((uint8_t)(reading>>8));
		USART_Transmit((uint8_t)reading);
		return 1;
	} else {
		return 3;
	}
}

void initialize(void) {
	USART_Init(0);	
	timer2_init(&trigger_function, &timestamp_function);
	adm1191_init();
	stdout = stdin = &uart_str;
	sensor_mask = 0;
	sei();
}


/* Main - a simple test program*/
int main(void) {
	uint8_t state;
	
	MCUSR = 0;
	wdt_disable();
	cli();
	
	state = 0;	
	for (;;) {		
		switch (state) {
		case 0: /* initialization */
			initialize();
			state = 1;
			break;

		case 1: /* interactive mode */
			state = get_command();
			break;

		case 2: /* initialize before collecting */
			wdt_enable(WD_TIMEOUT); /* enable watchdog timer with 1s timeout */
            init_reading_buffer();
			initialize_sensors();
			timer2_trigger_enable();
			state = 3;
			break;

		case 3:
            wdt_reset();
			state = check_for_done();
			break;

		default:
			state = state;
		}
	}
	return 0;
}
