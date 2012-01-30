#ifndef TIMER2_H_
#define TIMER2_H_

void timer2_init(void (*trigger_function_ptr)(), void(*timestamp_function_ptr)());
void timer2_set_trigger(uint16_t sample_per, uint32_t trigger_lim);
uint8_t timer2_get_trigger();
void timer2_set_time(uint32_t time);
uint32_t timer2_get_time(void);
void timer2_trigger_enable();
void timer2_trigger_disable();

#endif /*TIMER2_H_*/
