# reactionTest
This device uses the timer of the ATmega328p to accurately measure reaction time in response to either an LED light or a piezo buzzer.
Shows recorded time, average session time, and best session time on a simple 16x2 LCD display.
Written in C making use of the AVR-libc libraries.

While in the main menu, press c_sw to change between light and sound mode.
Press r_sw to begin the test or hold it to reset session times.
Press r_sw when the selected stimulus activates to test your reaction time.
Created in a group as part of an embedded systems class.
