/*
*
* RENCI
*
* File			: pmbl.c
* Project		: PowerMon Bootloader
* Description	: AVR109-compatible bootloader for PowerMon 2.
* Platform      : ATmega168
* Revision		: 0.1
* Date			: December 29, 2011
*
* License		: RENCI BSD License 1.0
*
* Portions derived from Atmel AVR Application Note 109
*
*/

#include <inttypes.h>
#include <avr/io.h>
#include <avr/boot.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

#define TX_TIMEOUT      10000
#define RX_TIMEOUT      10000
#define US32_SCALER     4

#define APP_MEM_END     14336 // 16384 minus bootloader size (in bytes)
#define PAGE_SIZE       128   // in words
#define BLOCK_SIZE      PAGE_SIZE

// signature for ATMega168
#define	SIGNATURE_BYTE_1 0x1E 
#define	SIGNATURE_BYTE_2 0x94
#define	SIGNATURE_BYTE_3 0x06

#define VERSION_HIGH '0'
#define VERSION_LOW  '1'

#ifndef F_CPU
    #define F_CPU       8000000
#endif

#define US_CYCLES   (F_CPU / 4000000)

// note RX_SAFE will overwrite rx
#define RX_SAFE(rx) {         \
    rx = uart_rx(RX_TIMEOUT); \
    if(rx == -1) {            \
      uart_tx('?');           \
      break;                  \
    }                         \
  }
  
void (*funcptr)(void) = 0x0000; //function pointer points to RESET vector

// prototype declarations
int main(void);
void uart_tx(uint8_t data);
uint8_t uart_hard_rx(uint16_t timeout);
uint8_t uart_soft_rx(uint16_t timeout);
void blockRead(uint16_t size, uint8_t mem, uint16_t *address);
uint8_t blockWrite(uint16_t size, uint8_t mem, uint16_t *address);

void uart_tx(uint8_t data) {    
    UDR0 = data;         
    // wait until data transmitted
    while(!(UCSR0A & (1 << TXC0)));
    UCSR0A |= TXC0; // clear flag
}

uint8_t uart_hard_rx(uint16_t timeout) { // timeout in milliseconds
    uint16_t elapsed = 0;
    while(!(UCSR0A & (1 << RXC0)) && (elapsed < timeout)) {
        if(TIFR0 & (1 << OCF0A)) {
            TCNT0 = 0;
            TIFR0 |= (1 << OCF0A);
            elapsed++;
        }
    }
    if(UCSR0A & (1 << RXC0)) {
        return UDR0;
    } else {
        wdt_enable(WDTO_15MS); //wd on,15ms 
        while(1);
        return 0;
    }
}

uint8_t uart_soft_rx(uint16_t timeout) { // timeout in milliseconds
    uint16_t elapsed = 0;
    while(!(UCSR0A & (1 << RXC0)) && (elapsed < timeout)) {
        if(TIFR0 & (1 << OCF0A)) {
            TCNT0 = 0;
            TIFR0 |= (1 << OCF0A);
            elapsed++;
        }
    }
    if(UCSR0A & (1 << RXC0)) {
        return UDR0;
    } else {
        return 0;
    }
}
          
int main(void) {  
    uint8_t seconds_left;
    uint16_t ms_left;
    uint16_t val;
    uint16_t tmp;
    uint16_t address = 0; //stored as a byte address, but often a word address

    tmp = MCUSR;
    MCUSR = 0;
    wdt_disable();

    cli(); // disable interrupts
    
    
    // init uart
    // set baud rate to 1000000
    UBRR0 = 0; 
    UCSR0A = 1 << U2X0;
    // enable UART rx and tx
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);
    // set 8N1
    UCSR0C = (0 << USBS0) | (1 << UCSZ01) | (1 << UCSZ00);
    DDRD = 1 << PD1;

    // init timer
    OCR0A = 125;
    TCNT0 = 0;
    TIFR0 |= (1 << OCF0A);
    TCCR0B = (1<<CS01) | (1<<CS00);

    //countdown
    seconds_left = 10; 
    ms_left = 0;
    uart_tx('\n');
    uart_tx('\r');
    uart_tx('\n');
    uart_tx('\r');  
    uart_tx((uint8_t)tmp);
    uart_tx('\n');
    uart_tx('\r');  
    while((seconds_left > 0) || (ms_left > 0)) {
        if(uart_soft_rx(1) == 0x1b) { //jump to program space
            break;
        }
        if(ms_left == 0) {
            seconds_left--;
            ms_left = 999;
            uart_tx('p');
            uart_tx('m');
            uart_tx('b');
            uart_tx('l');
            uart_tx(' ');
            uart_tx(seconds_left+'0');
            uart_tx('\n');
            uart_tx('\r');
        } else {
            ms_left--;
        }
    }
    
    if((seconds_left == 0) && (ms_left == 0)) {
        boot_rww_enable_safe();
        funcptr();
    }

    //bootloader loop
    while(1) {
        val=uart_soft_rx(RX_TIMEOUT); // Wait for command character.
        switch(val) {
            case 'a': //Check autoincrement support.
                uart_tx('Y'); //Report "yes".
                break;
            case 'b': //Check block support.
                uart_tx('Y'); //Report "yes".
                uart_tx((BLOCK_SIZE >> 8) & 0xFF);
                uart_tx(BLOCK_SIZE & 0xFF);
                break;                

            case 'A': //Set address.
                address = uart_hard_rx(RX_TIMEOUT);
                address <<= 8;
                address |= uart_hard_rx(RX_TIMEOUT);
                uart_tx('\r'); // send OK
                break;

            case 'e': //Erase chip.
                for(address = 0; address < APP_MEM_END; address += PAGE_SIZE) {
                    boot_page_erase_safe(address);
                }
                uart_tx('\r'); // send OK
                break;

            case 'R': //Read program memory.
                boot_rww_enable_safe();
                uart_tx(pgm_read_byte((address << 1)+1));
                uart_tx(pgm_read_byte((address << 1)+0));
                address++;
                break;
            case 'c': //Write low byte to program memory. (Always goes first.)
                tmp = uart_hard_rx(RX_TIMEOUT);
                uart_tx('\r'); //send OK.                
                break;
            case 'C': //Write high byte to program memory.
                tmp |= (uart_hard_rx(RX_TIMEOUT) << 8);
                boot_page_fill_safe(address << 1, tmp);
                address++;
                uart_tx('\r');
                break;
            case 'm': //Write page.
                if(address >= (APP_MEM_END >> 1)) { 
                    uart_tx('?');                
                } else {
                    boot_page_write_safe(address << 1);
                    uart_tx('\r'); //Send ok.
                }
                break;

            case 'd': //Read EEPROM
                uart_tx(eeprom_read_byte((uint8_t *)address));
                address++;
                break;
            case 'D': //Write EEPROM.                
                tmp = uart_hard_rx(RX_TIMEOUT);
                boot_spm_busy_wait();                
//                EEARL = address;
//                EEARH = (address >> 8);
//                EECR |= (1<<EEMPE);
//                EECR |= (1<<EEPE);
//                while(EECR & (1<<EEPE));
//                address++;
                eeprom_write_byte((uint8_t *)address, (uint8_t)tmp);
                eeprom_busy_wait();
                address++;
                uart_tx('\r'); //Send OK.
                break;

            case 'g': //Start block read.
                // Get block size;
                tmp = uart_hard_rx(RX_TIMEOUT);
                tmp <<= 8;
                tmp |= uart_hard_rx(RX_TIMEOUT);
                val = uart_hard_rx(RX_TIMEOUT);
                blockRead(tmp,val,&address); // Do block read.
                break;
            case 'B': //Start block write.
                // Get block size;
                tmp = uart_hard_rx(RX_TIMEOUT);
                tmp <<= 8;
                tmp |= uart_hard_rx(RX_TIMEOUT);
                val = uart_hard_rx(RX_TIMEOUT);
                uart_tx(blockWrite(tmp,val,&address)); // Do block load, return result.
                break;

            case 'F': //Read low fuse bits.
                boot_spm_busy_wait();
                tmp = boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS);
                uart_tx((uint8_t)tmp);
                break;
            case 'N': //Read high fuse bits.
                boot_spm_busy_wait();
                tmp = boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS);
                uart_tx((uint8_t)tmp);
                break;
            case 'Q': //Read extended fuse bits.
                boot_spm_busy_wait();
                tmp = boot_lock_fuse_bits_get(GET_EXTENDED_FUSE_BITS);
                uart_tx((uint8_t)tmp);
                break;
            case 'r': //Read lock bits.
                boot_spm_busy_wait();
                tmp = boot_lock_fuse_bits_get(GET_LOCK_BITS);
                uart_tx((uint8_t)tmp);
                break;
            case 'l': //Set lock bits.
                tmp = uart_hard_rx(RX_TIMEOUT);
                boot_lock_bits_set_safe(~((uint8_t)tmp));
                uart_tx('\r'); // Send OK.
                break;

            case 'E': //Exit bootloader.
                uart_tx('\r');
                boot_rww_enable_safe();
                funcptr(); // XXX
                break;

            case 'p'://Get programmer type.
                uart_tx('S'); //'SERIAL'
                break;
            case 's': //Get signature bytes
                uart_tx(SIGNATURE_BYTE_3);
                uart_tx(SIGNATURE_BYTE_2);
                uart_tx(SIGNATURE_BYTE_1);
                break;
            case 't'://Get supported device codes.
                uart_tx(0);
                break;
            case 'S': //Return programmer ID
                uart_tx('P');
                uart_tx('M');
                uart_tx('B');
                uart_tx('L');
                uart_tx(' ');
                uart_tx(' ');
                uart_tx(' ');
                break;
            case 'V':
                uart_tx(VERSION_HIGH);
                uart_tx(VERSION_LOW);
                break;

            case 'L': //Leave programming mode.
            case 'P': //Enter programming mode.
                uart_tx('\r'); //Send OK.
                break;

            case 'x': //set LED
            case 'y': //clear LED
            case 'T': //Set device type.
                uart_soft_rx(RX_TIMEOUT); //ignore
                uart_tx('\r');//Send OK.
                break;

            case 0x1b: //sync
            case 0:   //default
                break;
           
            default: //anything else
                uart_tx('?');
        } // switch        
      } // while    
} //main

void blockRead(uint16_t size, uint8_t mem, uint16_t *address) {    
    if(mem=='E') { //EEPROM    
        while(size > 0) {
            uart_tx(eeprom_read_byte((uint8_t *)(*address)));
            (*address)++;
            size--;     
        }        
    } else if(mem=='F') { //Flash
        while(size > 0) {
            boot_rww_enable_safe();
            uart_tx(pgm_read_byte((*address) << 1));
            uart_tx(pgm_read_byte(((*address) << 1)+1));
            (*address)++;
            size -= 2;
        }
    } else {
        uart_tx('?');
    }
}

// size in bytes
// mem = EEPROM or Flash
uint8_t blockWrite(uint16_t size, uint8_t mem, uint16_t *address) {
    uint8_t buffer[BLOCK_SIZE];
    uint16_t tmp;
    uint16_t data;
    uint16_t temp_address;
//    uint16_t rx;
	
    // EEPROM 
    if(mem=='E') {        
        // Fill buffer.
        for(tmp=0;tmp<size;tmp++) {
            data = uart_hard_rx(RX_TIMEOUT);
//            if(rx == -1) {
//                return '?';
//            }
            buffer[tmp] = (uint8_t)data;
        }
        // Then program.
        boot_spm_busy_wait();
    	for(tmp=0;tmp<size;tmp++) {
            eeprom_write_byte((uint8_t *)(*address), buffer[tmp]);
            eeprom_busy_wait();
            (*address)++;
        }
        return '\r'; //Return OK
    //Flash
    } else if(mem=='F') {
        (*address) <<= 1;
        temp_address = (*address);
        while(size) {
            data = uart_hard_rx(RX_TIMEOUT);
//            if(rx == -1) {
//                return '?';
//            }
//            data = (rx & 0xFF);
//            rx = uart_rx(RX_TIMEOUT);
//            if(rx == -1) {
//                return '?';
//            }
            tmp = uart_hard_rx(RX_TIMEOUT);
            tmp <<= 8;
            data |= tmp;            
            boot_page_fill_safe(*address, data);
            (*address) += 2;
            size -= 2;
        }
        boot_page_write_safe(temp_address);
//        boot_rww_enable_safe(); //I don't think this is necessary.
        (*address) >>= 1;
        return '\r'; // Return OK
    } else { //invalid memory type
        return '?';
    }
}
