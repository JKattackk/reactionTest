/*
 * File:   newavr-main.c
 *
 * Created on November 24, 2022, 12:16 PM
 */


#define F_CPU 11059200UL
#define CONSTANT 0.02314814815 //corresponds to the step size of timer one, in ms
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <stdio.h>
#include <stdarg.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#include "hd44780.h"
#include "lcd.h"
#include "defines.h"

FILE lcd_str = FDEV_SETUP_STREAM(lcd_putchar, NULL, _FDEV_SETUP_WRITE);

float min = 0;
float average = 0;
unsigned int count = 0;
volatile unsigned int overflows;
int mode = 1;

volatile bool early = false;

//sets up timer0 for output to the piezo buzzer.
//sets up timer1 for measuring time.

void timer_init() {
    //T0 setup
    TCCR0A |= (1 << WGM01); //CTC mode, top of OCRA
    TCCR0B |= (1 << CS01); //clock divided by eight
    OCR0A = 70; //adjusting this value will change the frequency of the buzzer tone.

    //T1 setup
    TCCR1B |= (1 << CS12) | (1 << ICES1);   //divide by 256. rising edge of r_switch triggers input capture
    TIMSK1 |= (1 << TOIE1);  //enable timer overflow interrupt
}

//takes two timer values, calculates and displays current reading, average, min
void updateTime(unsigned int t0,unsigned int t1,unsigned int overflows) {
    float measurement;

    //calculate current measurement
    measurement = ((65536 * overflows + t1) - t0) * CONSTANT;

    //update measurement on display
    cli();
    printf("\x1b\x80    %06.1fms   ", measurement);
    count++;

    //calculate new average
    if ((measurement < min) | (min == 0)) {
    min = measurement;
    }
    //calculate new average
    average = ((count - 1) * average) / count + (measurement / count);


    printf("\x1b\xc0%06.1f, %06.1fms", average, min);
}

void runTest(int mode) {
START:
    cli();
    printf("\x1b\x80  running...");

    unsigned int t1, t0;
    
    //this loop delays the start of the test by 2 seconds + a random time
    //response button is checked during this delay so that if the button is pressed before the light/buzzer turns on the test gets reset.
    srand(TCNT1);
    int delay = 1000 + rand()/4;
    while (delay > 0) {
        if (PINB & (1 << PB0)) {
            printf("\x1b\x80   too early");
            while (PINB & (1 << PB0)) {
            }
            _delay_ms(200);
            goto START;
        }
        _delay_ms(1);
        delay = delay - 1;
    }
    
    //turn on light/buzzer dependent on mode
    if (mode) {
        PORTD ^= (1 << PD0); //toggle light
    } else {//toggle connection between timer and piezo buzzer
        TCCR0A ^= (1 << COM0A0);
    }
    t0 = TCNT1; //start time
    TIFR1 |= (1 << ICF1) | (1 << TOV1); //clear flag
    overflows = 0;
    sei();
    //waiting for response button to be pressed, times out after x seconds if no response
    while (overflows < 6) {
        if (!(TIFR1 & (1 << ICF1))) {
        } else {
            //end time, calculate response time using t0, t1, and number of overflows
            t1 = ICR1;
            updateTime(t0, t1, overflows);
            break;
        }
    }
    
    //turn light/buzzer back off
    if (mode) {
        PORTD ^= (1 << PD0); //toggle light
    } else {//toggle connection between timer and piezo buzzer
        TCCR0A ^= (1 << COM0A0);
    }
    //if previous loop timed out, print no response.
    if (overflows >= 6) {
        cli();
        printf("\x1b\x80  No Response");
        _delay_ms(200);
        printf("\x1b\x80      NANms   ");
    }
    while (PINB & (1 << PB0)) {
    }
    EIFR = (1 << INTF0);
    sei();
}

//resets the stored values and resets the LCD display
void resetValues() {
    printf("\x1b\x01\x1b\x80      NANms    L");
    printf("\x1b\xc0  NANms    NANms");
    average = 0;
    min = 0;
    count = 0;
    mode = 1;
}

int main(void) {
    //outputs for LED and buzzer
    DDRD |= (1 << PD0);
    DDRD |= (1 << PD6);

    DDRD &= ~(1 << PD2);
    PORTD &= ~(1 << PD2);
    
    timer_init();
    
    //LCD setup, startup display
    lcd_init();
    stdout = &lcd_str;
    printf("\x1b\x01\x1b\x80      NANms    L");
    printf("\x1b\xc0  NANms    NANms");
    
    //
    EICRA |= (1 << ISC00);
    //EIFR = (1 << INTF0);
    sei();
    while (1) {
        EIMSK |= (1 << INT0);
        //if response pin pressed

        if (PINB & (1 << PB0)) {
            EIMSK &= ~(1 << INT0);
            while (PINB & (1 << PB0)) {
            }
            runTest(mode);
        }
    }
}

ISR(TIMER1_OVF_vect, ISR_BLOCK) {
    overflows++;
}

ISR(INT0_vect) {
    int delay = 1000;
    while (delay > 0) {
        if (EIFR & (1 << INTF0)) {
            EIFR = (1 << INTF0);
            mode = !mode;
            if (mode) {
                printf("\x1b\x8FL");
            } else {
                printf("\x1b\x8FS");
            }
            goto END;
        }
        _delay_ms(1);
        delay--;
    }
    resetValues();
END:
    EIFR = (1 << INTF0);
}