#include <stdio.h>
#include <inttypes.h>
#include <avr/io.h>
#include <util/twi.h>

#define ADDRESS 0x74

void adm1191_init();
uint8_t adm1191_get(uint8_t *dest, uint8_t remaining_length, uint8_t address);

void adm1191_init() {
	DDRC |= (1 << PC0) | (1 << PC1) | (1 << PC2) | (1 << PC3);
	DDRD |= (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7);
	DDRC &= ~(1 << PC5 | 1<< PC4);
	
	PORTC |= (1 << PC0) | (1 << PC1) | (1 << PC2) | (1 << PC3); /* enable triggers */
	PORTD |= (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7);	/* enable triggers */
 	PORTC |= (1 << PC5) | (1 << PC4);  /* enable pullups for scl and sda*/

 	TWSR = 0; /* no prescaler */
	TWBR = 2; /* SCL freq = 8 / 22 = .363 MHz */
}

uint8_t adm1191_put(uint8_t *src, uint8_t remaining_length, uint8_t address) {
	uint8_t transmitted_length = 0;
	uint8_t twst;

	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN); /* send start condition */
	while (!(TWCR & (1 << TWINT))); /* wait */

	switch ((twst = TW_STATUS)) 
	{
	case TW_REP_START:
	case TW_START:
/*		printf("  i2c sent start\r\n");*/
		break;
	default:
		return 0;
	}

	TWDR = address | TW_WRITE; /* address + read */
	TWCR = (1 << TWINT) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)));
/*	printf("  i2c sent %#x\r\n", (address | TW_WRITE));*/
	
	switch ((twst = TW_STATUS)) 
	{
	case TW_MT_SLA_ACK:
/*		printf("  i2c received address ack\r\n");*/
		break;

	default:
/*		printf("  i2c status = %#x\r\n", twst);*/
		TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN); /* send stop */
		return 0;
	}
	
	while( remaining_length > 0 ) {
		TWDR = src[transmitted_length++];
		remaining_length--;
		TWCR = (1 << TWINT) | (1 << TWEN); /* enable transmission */
		while (!(TWCR & (1 << TWINT))); /* wait */
		switch((twst = TW_STATUS))
		{
		case TW_MT_DATA_ACK:
			break;
		case TW_MT_DATA_NACK:
			return 0;
		default:
			TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN); /* send stop */			
			return 0;
		}
	}
	return transmitted_length;
}

uint8_t adm1191_get(uint8_t *dest, uint8_t remaining_length, uint8_t address) {
	uint8_t received_length = 0;
	uint8_t twst;
	
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN); /* send start condition */
	while (!(TWCR & (1 << TWINT))); /* wait */

	switch ((twst = TW_STATUS)) 
	{
	case TW_REP_START:
	case TW_START:
/*		printf("  i2c sent start\r\n");*/
		break;
	default:
		return 0;
	}

	TWDR = address | TW_READ; /* address + read */
	TWCR = (1 << TWINT) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)));
/*	printf("  i2c sent %#x\r\n", (address | TW_READ)); */
	
	switch ((twst = TW_STATUS)) 
	{
	case TW_MR_SLA_ACK:
/*		printf("  i2c received address ack\r\n");*/
		break;

	default:
/*		printf("  i2c status = %#x\r\n", twst);*/
		return 0;
	}
	
	while( remaining_length > 0 ) {
		if( remaining_length == 1) {
			TWCR = (1 << TWINT) | (1 << TWEN); /* send NACK */
		} else {
			TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA); /* send ACK */
		}
		while (!(TWCR & (1 << TWINT)));
		
		switch ((twst = TW_STATUS))
		{
		case TW_MR_DATA_NACK:
		case TW_MR_DATA_ACK:
			remaining_length--;
			*dest++ = TWDR;
			received_length++;
			break;
		
		default:
			TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN); /* send stop */
			return 0;
		}
	}
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN); /* send stop */
	return received_length;
}
