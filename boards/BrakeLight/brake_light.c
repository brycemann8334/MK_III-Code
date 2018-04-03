/**********************************************************
This is the base document for getting started with writing
firmware for OEM.
TODO delete before submitting
**********************************************************/

/*
Header:
    Explain what this project does as overview
Author:
    @author
*/


/* TODO */
/*
    - Add sensing display integration
    - Add brake position sending
/*

/*----- Includes -----*/
#include <stdio.h>
#include <stdlib.io>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>

/*----- Macro Definitions -----*/
/* Shutdown */
#define GLOBAL_SHUTDOWN         0x0

/* Brake */
#define BRAKE_PIN               PC7 //TODO Analog Brake INPUT
#define BRAKE_PORT              PORTC //TODO
#define BRAKE_LIGHT_PIN         PD2 //TODO Analog Brake Light OUTPUT
#define BRAKE_LIGHT_PORT        PORTD //TODO

/* BSPD Status Output */
#define BSPD_STATUS_PIN         PC3 //TODO
#define BSPD_STATUS_PORT        PORTC //TODO

/* Sense Lines */
#define SD_MAIN_FUSE		PB0
#define SD_LEFT_E_STOP		PB1
#define SD_RIGHT_E_STOP		PD5
#define SD_BSPD     		PD6
#define SD_HVD      		PD7
#define SD_TSMS     		PB2

#define PORT_MAIN_FUSE      PORTB
#define PORT_LEFT_E_STOP    PORTB
#define PORT_RIGHT_E_STOP   PORTD
#define PORT_BSPD           PORTD
#define PORT_HVD            PORTD
#define PORT_TSMS           PORTB

// Brake
#define BRAKE_PIN           PB5;
#define PORT_BRAKE          PORTB;

/* CAN Positions */
#define CAN_BRAKE           0
#define CAN_BREAK_POS       1
#define CAN_BSPD            2
#define CAN_HVD             3
#define CAN_TSMS            4
#define CAN_LEFT_E_STOP     5
#define CAN_RIGHT_E_STOP    6
#define CAN_MAIN_FUSE       7

#define BROADCAST_MOb       0



/* Sense LEDs */
// Might be irrelevant because the gStatusBar
#define EXT_LED_GREEN           PD0 //(Debug LED on RJ45)
#define EXT_LED_ORANGE          PC0 //(Debug LED on RJ45)
#define LED1                    PC5 //TODO (Purpose - on LED bar)
#define LED2                    PC4 //TODO (Purpose - on LED bar)

#define PORT_EXT_LED_GREEN      PORTD //TODO (Debug LED on RJ45)
#define PORT_EXT_LED_ORANGE    PORTC //TODO (Debug LED on RJ45)
#define PORT_LED1               PORTC //TODO (Purpose - on LED bar)
#define PORT_LED2               PORTC //TODO (Purpose - on LED bar)


#define STATUS_MAIN_FUSE    1
#define STATUS_LEFT_E_STOP  2
#define STATUS_RIGHT_E_STOP 3
#define STATUS_BSPD         4
#define STATUS_HVD          5
#define STATUS_TSMS         6
#define STATUS_BRAKE        7



/*----- Global Variables -----*/
volatile uint8_t gFlag = 0x01;  // Global Flag
unit8_t gCANMessage[8] = {0, 0, 0, 0, 0, 0, 0, 0};  // CAN Message
unit8_t gPRECHARGE_TIMER = 0x00;

volatile unit8_t gTSMS = 0x00;
volatile unit8_t gTSMS_OLD = 0x00;  // Used for comparison

#define UPDATE_STATUS   0       // Determines when to send messages
#define TSMS_STATUS     1       // Used to track changes in TSMS

unit8_t clock_prescale = 0x00;  // Used for timer

/*----- Interrupt(s) -----*/
// CAN
ISR(CAN_INT_vect) {
    // TSMS
    CANPAGE = (0 << MOBNB0); //TODO correct page
    if(bit_is_set(CANSTMOB, RXOK)) {
        volatile unit8_t msg = CANSMG;          //TODO figure out order of msg
        gTSMS = msg;
        if(msg == 0x00) {
            gFlags &= ~_BV(TSMS_STATUS)         // Clear bit
        } else {
            gFlags |= _BV(TSMS_STATUS)          // Set bit
        }
    }
}

ISR(PCINT0_vect) {
    /*
    Standard Pin Change Interupt
    covers interupts 0-2
    Interupts covered: Main Shutdown Fuse, Left E-Stop, TSMS, & Brake Light
    */
    if(PORT_MAIN_FUSE, SD_MAIN_FUSE) {
        gFlag |= _BV(STATUS_MAIN_FUSE);
    } else {
        gFlag &= ~_BV(STATUS_MAIN_FUSE);
    }
    if(PORT_LEFT_E_STOP, SD_LEFT_E_STOP) {
        gFlag |= _BV(STATUS_LEFT_E_STOP);
    } else {
        gFlag &= ~_BV(STATUS_LEFT_E_STOP);
    }
    if(PORT_TSMS, SD_TSMS) {
        gFlag |= _BV(STATUS_TSMS);
    } else {
        gFlag &= ~_BV(STATUS_TSMS);
    }
    if(PORT_BRAKE, PIN_BRAKE) {
        gFlag |= _BV(STATUS_BRAKE);
    } else {
        gFlag &= ~_BV(STATUS_BRAKE);
    }
}

ISR(PCINT2_vect) {
    /*
    Standard Pin Change Interupt
    covers interupts 21-23
    Interupts covered: Right E-Stop, BSPD, HVD
    */
    if(PORT_RIGHT_E_STOP, SD_RIGHT_E_STOP) {
        gFlag |= _BV(STATUS_RIGHT_E_STOP);
    } else {
        gFlag &= ~_BV(STATUS_RIGHT_E_STOP);
    }
    if(PORT_BSPD, PORT_BSPD) {
        gFlag |= _BV(STATUS_BSPD);
    } else {
        gFlag &= -_BV(STATUS_BSPD);
    }
    if(PORT_HVD, PORT_HVD) {
        gFlag |= _BV(STATUS_HVD);
    } else {
        gFlag &= -_BV(STATUS_HVD);
    }
}

// 8-bit Timer
ISR(TIMER0_COMPA_vect) {
    // Only send CAN msgs every 20 cycles
    if( clock_prescale > 20 ) {
        gFlag |= _BV(UPDATE_STATUS);
        clock_prescale = 0;
    }
    clock_prescale++;
}


/*----- Functions -----*/
void initTimer_8bit(void) {
    TCCR0A = _BV(WGM01);    // Set up 8-bit timer in CTC mode
    TCCR0B = 0x05;          // clkio/1024 prescaler
    TIMSK0 |= _BV(OCIE0A);
    OCR0A = 0xFF;
}

static inline void read_pins(void) {

    /* Build CAN Message */
    if(bit_is_clear(gFlag, STATUS_MAIN_FUSE)) {
        gCAN_MSG[CAN_MAIN_FUSE] = 0xFF;     // Electrical signal is low (meaning fuse is set)
    } else {
        gCAN_MSG[CAN_MAIN_FUSE] = 0x00;
    }

    if(bit_is_clear(gFlag, STATUS_LEFT_E_STOP)) {
        gCAN_MSG[CAN_LEFT_E_STOP] = 0xFF;
    } else {
        gCAN_MSG[CAN_LEFT_E_STOP] = 0x00;
    }

    if(bit_is_clear(gFlag, STATUS_RIGHT_E_STOP)) {
        gCAN_MSG[CAN_RIGHT_E_STOP] = 0xFF;
    } else {
        gCAN_MSG[CAN_RIGHT_E_STOP] = 0x00;
    }

    if(bit_is_clear(gFlag, STATUS_BSPD)) {
        gCAN_MSG[CAN_BSPD] = 0xFF;
    } else {
        gCAN_MSG[CAN_BSPD] = 0x00;
    }

    if(bit_is_clear(gFlag, STATUS_HVD)) {
        gCAN_MSG[CAN_HVD] = 0xFF;
    } else {
        gCAN_MSG[CAN_HVD] = 0x00;
    }

    if(bit_is_clear(gFlag, STATUS_TSMS)) {
        gCAN_MSG[CAN_TSMS] = 0xFF;
    } else {
        gCAN_MSG[CAN_TSMS] = 0x00;
    }

    if(bit_is_clear(gFlag, STATUS_BRAKE)) {
        gCAN_MSG[CAN_BRAKE] = 0xFF;
    } else {
        gCAN_MSG[CAN_BRAKE] = 0x00;
    }
}

void checkShutdownState(void)   {
    /*
    -Check if bits are set
        -IF they are, set CAN list position to 0xFF
        -ELSE do set CAN list position to 0x00
    */
}

void updateStateFromFlags(void) {
    /*
    Based off the state of the flag(s), update components and send CAN
    */
}

//TODO any other functionality goes here
/*
    MISSING MODULE
    TODO
    Update LED status bar
*/


/*----- MAIN -----*/
int main(void){
    /*
    -Set up I/O
    -Set up CAN timer (done)
    -Initialize external libraries (if applicable)
    -Wait on CAN
    -Infinite loop checking shutdown state!
    */
    sei();                              // Enable interrupts

    /* Setup interrupt registers */
    PCICR |= _BV(PCIE0) | _BV(PCI2);
    PCMSK0 |= _BV(PCINT0) | _BV(PCINT1) | _BV(PCINT2) | _BV(PCINT5);      // Covers Pins: Main Fuse, Left E-Stop, TSMS, & Brake Light
    PCMSK2 |= _BV(PCINT21) | _BV(PCINT22) | _BV(PCINT23);   // Covers Pins: Right E-Stop, BSPD, and HVD

    initTimer_8bit();                   // Begin 8-bit timer

    gFlag |= _BV(UPDATE_STATUS);        // Read ports

    while(1) {
        if(bit_is_set(gFlag, UPDATE_STATUS)) {
            PORT_EXT_LED_ORANGE ^= _BV(EXT_LED_ORANGE);     // Blink Orange LED for timing check

            read_pins();                // Update all pin values

            CAN_transmit(BROADCAST_MOb, CAN_ID_BRAKE_LIGHT,
                CAN_LEN_BRAKE_LIGHT, gCAN_MSG);

            gFlag &= ~_BV(UPDATE_STATUS);
        }
    }
}