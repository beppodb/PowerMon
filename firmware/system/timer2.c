/* timer2
 * facilitates connection of 32 kHz clock
 * holds time in a 32-bit register
 */

#include <avr/interrupt.h>
#include <inttypes.h>
#include <stdio.h>
#include "timer2.h"

static volatile uint32_t current_time;
uint32_t trigger_limit;
static volatile uint32_t trigger_count;
uint16_t partial_time;
uint16_t sample_clock;
uint16_t sample_period;
static volatile uint8_t trigger_enable;

void (*timestamp_function)(uint32_t time);
void (*trigger_function)();

ISR(TIMER2_COMPA_vect) {
//	cli();	
	partial_time++;
	if(trigger_enable) {
		sample_clock++;
		if(sample_clock == sample_period) {
			trigger_function();
			if(++trigger_count == trigger_limit) {
				trigger_enable = 0;
			}	
			sample_clock = 0;
		}
	}
	if(partial_time == 1024) {
		++current_time;
		if(trigger_enable) {
			timestamp_function(current_time);
		}
		partial_time = 0;
	}
//	sei();
}

void timer2_set_trigger(uint16_t sample_per, uint32_t trigger_lim) {
	sample_period = sample_per;
	trigger_limit = trigger_lim;
	trigger_count = 0;
	sample_clock = 0;
}

uint8_t timer2_get_trigger() {
	return trigger_enable;
}

void timer2_trigger_enable() {
	trigger_enable = 1;
	trigger_count = 0;
	sample_clock = 0;
}

void timer2_trigger_disable() {
	trigger_enable = 0;
}

void timer2_set_time(uint32_t time) {
	TCNT2 = 0; /* reset timer */
	GTCCR = 1 << PSRASY; /* reset prescaler */
	sample_clock = 0;
	partial_time = 0;
	current_time = time;
}

void timer2_init(void(*trigger_function_ptr)(), void(*timestamp_function_ptr)(uint32_t time)) {
	trigger_function = trigger_function_ptr;
	timestamp_function = timestamp_function_ptr;

	
	
	GTCCR = 1 << PSRASY; /* reset prescaler */	

	/* disable interrupts */
	TIMSK2 = 0; /* disable timer interrupts */
	/* select cs */	
	ASSR = 1 << AS2; /* no asynchronous clock */
	/* write to TCNT2 */
	TCNT2 = 0; /* reset timer */
	/* wait for TCN2xUB */
	while(ASSR & (1 << TCN2UB));
	/* clear flags */
	TIFR2 = 0; /* clear flags */
	/* enable interrupts */
	OCR2A = 31;
	TCCR2A = 1 << WGM21; /* no compare outputs, normal waveform, clear timer on compare match */
	TCCR2B = 0; /* no forced output, normal waveform, clock stopped */	
	
	TIMSK2 = 1 << OCIE2A; /* interrupt on compare match a */
	
	sample_clock = 0;
	sample_period = 0;
	trigger_enable = 0;
	partial_time = 0;
	current_time = 0;
	
	TCCR2B = 1 << CS20; /* no prescalar */

//	TCCR2B = 1 << CS20; /*1/8 prescalar*/ /* | 1 << CS21; */ /* 1/256 prescaler */
}

uint32_t timer2_get_time(void) {
	return current_time;
}
