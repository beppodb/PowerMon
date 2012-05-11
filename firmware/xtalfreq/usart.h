/* usart.h, based on AVR306, USART1.c */

#ifndef USART_H_
#define USART_H_

#define USART_RX_BUFFER_SIZE 32    /* 2,4,8,16,32,64,128 or 256 bytes */
#define USART_TX_BUFFER_SIZE 32	/* 2,4,8,16,32,64,128 or 256 bytes */
#define USART_RX_BUFFER_MASK ( USART_RX_BUFFER_SIZE - 1 )
#define USART_TX_BUFFER_MASK ( USART_TX_BUFFER_SIZE - 1 )

#if ( USART_RX_BUFFER_SIZE & USART_RX_BUFFER_MASK )
	#error RX buffer size is not a power of 2
#endif
#if ( USART_TX_BUFFER_SIZE & USART_TX_BUFFER_MASK )
	#error TX buffer size is not a power of 2
#endif

/* Prototypes */
void USART_Init( unsigned int baudrate );
int USART_Receive( void );
int USART_Transmit( unsigned char data );
unsigned char DataInReceiveBuffer( void );

#endif /*USART_H_*/
