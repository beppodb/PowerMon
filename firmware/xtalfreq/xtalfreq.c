#include <stdio.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include "usart.h"

FILE uart_str = FDEV_SETUP_STREAM((int(*)(char, FILE *))USART_Transmit, (int(*)(FILE *))USART_Receive, _FDEV_SETUP_RW);

volatile uint16_t frequency;
volatile uint16_t last_frequency, countdown;
volatile uint8_t write;

ISR(TIMER0_OVF_vect) {    
    if(!(--countdown)) {
        last_frequency = frequency;
        frequency = 0;
        countdown = 31250;
        write = 1;
    }
    return;
}

ISR(TIMER2_COMPA_vect) {        ;
    frequency+=2;
    return;
}

void init(void) {
    last_frequency = 0;
    frequency = 0;
    countdown = 31250;
    write = 0;

    TCCR0A = 0;
    TCCR0B = 0;
    TCNT0 = 0;
    TIMSK0 = (1 << TOIE0);
    TCCR0B = 0;
  
    ASSR = 1 << AS2;
    TCCR2A = (1 << COM2A1) | (1 << COM2A0) | (1 << WGM21) | (1 << WGM20);
    TCCR2B = (1 << WGM22);
    while(ASSR & (1 << TCN2UB));
    TCNT2 = 0;
    while(ASSR & (1 << OCR2AUB));
    OCR2A = 1;
    TIMSK2 = (1 << OCIE2A);

	USART_Init(0);	
    stdout = stdin = &uart_str;
    TCCR2B |= (1 << CS00);
    TCCR0B |= (1 << CS00);
}

int main(void) {
    int got;

	MCUSR = 0;
	wdt_disable();

    cli();
    init();
    sei();
    printf("OK\n");
    while(1) {
        if(write == 1) {            
            printf("Frequency = %u\n\r",(unsigned int)last_frequency);
            write = 0;
        } else {
            if(DataInReceiveBuffer()) {
                got = fgetc(stdin);
                if(got == 'P') {
        	        wdt_enable(WDTO_15MS);
                    while(1);             
                }
            }
        }
    }
}
