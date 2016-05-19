#include "uart.h"
#include "cbuffer.h"
#include <xc.h>

#define BUF_WIDTH_IN_BITS   8

volatile static Buffer txBuf;
volatile static Buffer rxBuf;
volatile static uint8_t txBufArr[TX_BUF_LENGTH];
volatile static uint8_t rxBufArr[RX_BUF_LENGTH];
volatile static uint8_t writeLock = 0;
volatile static uint8_t readLock = 0;

void UART_init(void){
    ANSBbits.ANSB2 = ANSBbits.ANSB7 = 0;
    
    BUF_init((Buffer*)&txBuf, (uint8_t*)txBufArr, TX_BUF_LENGTH, BUF_WIDTH_IN_BITS);
    BUF_init((Buffer*)&rxBuf, (uint8_t*)rxBufArr, RX_BUF_LENGTH, BUF_WIDTH_IN_BITS);
    
    /* baud rate = 57600bps 
     * U1BRG = (12000000/(16*57600)) - 1 = 12.02 = 12
     */
    U1BRG = 12;
    U1MODE = 0x0000;    /* TX/RX only, standard mode */
    U1STA = 0x0000;     /* enable */
    
    /* uart interrupts */
    IFS0bits.U1TXIF = IFS0bits.U1RXIF = 0;
    IEC0bits.U1TXIE = IEC0bits.U1RXIE = 1;
    
    U1MODEbits.UARTEN = 1;
    U1STAbits.UTXEN = 1;
    
    return;
}

void UART_read(uint8_t* data, uint16_t length){
    uint32_t i = 0;
    
    while(i < length){
        readLock = 1;
        data[i] = BUF_read8((Buffer*)&rxBuf);
        readLock = 0;
        i++;
    }
    
    /* if the read lock was active and something was received, 
     * then the RX interrupt was not properly executed and the
     * code must read the U1RXBUF into the circular buffer */
    readLock = 1;
    while((BUF_status((Buffer*)&rxBuf) != BUFFER_FULL)
            && (U1STAbits.URXDA)){
        BUF_write8((Buffer*)&rxBuf, U1RXREG);
    }
    readLock = 0;
}

void UART_write(uint8_t* data, uint16_t length){
    uint32_t i = 0;
    
    while(i < length){
        while(UART_writeable() == 0);   /* wait for any current writes to clear */

        writeLock = 1;        
        BUF_write8((Buffer*)&txBuf, data[i]);
        writeLock = 0;
        
        i++;
    }
    
    /* if the transmit isn't active, then kick-start the
     * transmit; the interrupt routine will finish sending
     * the remainder of the buffer */
    writeLock = 1;
    while((U1STAbits.UTXBF == 0) && (BUF_status((Buffer*)&txBuf) != BUFFER_EMPTY)){
        U1TXREG = BUF_read8((Buffer*)&txBuf);
    }
    writeLock = 0;
}

uint16_t UART_readable(void){
    return (uint16_t)BUF_fullSlots((Buffer*)&rxBuf);
}

uint16_t UART_writeable(void){
    return (uint16_t)BUF_emptySlots((Buffer*)&txBuf);
}

void _ISR _U1TXInterrupt(void){
    if(writeLock == 0){
        /* read the byte(s) to be transmitted from the tx circular
         * buffer and transmit using the hardware register */
        while((BUF_status((Buffer*)&txBuf) != BUFFER_EMPTY)
                && (U1STAbits.UTXBF == 0)){

            U1TXREG = BUF_read8((Buffer*)&txBuf);
        }
    }
    
    IFS0bits.U1TXIF = 0;
}

void _ISR _U1RXInterrupt(void){
    /* read the received byte(s) from the register and write
     * to the rx circular buffer */
    if(readLock == 0){
        while((BUF_status((Buffer*)&rxBuf) != BUFFER_FULL)
                && (U1STAbits.URXDA)){
            BUF_write8((Buffer*)&rxBuf, U1RXREG);
        }
    }
    
    
    IFS0bits.U1RXIF = 0;
}


