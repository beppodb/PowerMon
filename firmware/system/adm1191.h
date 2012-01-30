#ifndef ADM1191_H_
#define ADM1191_H_

void adm1191_init();
uint8_t adm1191_get(uint8_t *dest, uint8_t remaining_length, uint8_t address);
uint8_t adm1191_put(uint8_t *src, uint8_t remaining_length, uint8_t address);

#endif /*ADM1191_H_*/
