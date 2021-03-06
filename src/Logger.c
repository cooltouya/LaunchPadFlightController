/* Copyright (C) 2015 Kristian Lauszus, TKJ Electronics. All rights reserved.

 This software may be distributed and modified under the terms of the GNU
 General Public License version 2 (GPL2) as published by the Free Software
 Foundation and appearing in the file GPL2.TXT included in the packaging of
 this file. Please note that GPL2 Section 2[b] requires that all works based
 on this software must also be made publicly available under the terms of
 the GPL2 ("Copyleft").

 Contact information
 -------------------

 Kristian Lauszus, TKJ Electronics
 Web      :  http://www.tkjelectronics.com
 e-mail   :  kristianl@tkjelectronics.com
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#if STEP_ACRO_SELF_LEVEL || STEP_ALTITUDE_HOLD || STEP_HEADING_HOLD

#include "Logger.h"
#include "Pins.h"
#include "Time.h"
#include "uartstdio1.h" // Add "UART_BUFFERED1" to preprocessor - it uses a modified version of uartstdio, so it can be used with another UART interface

#include "driverlib/gpio.h"
#include "inc/hw_memmap.h"

typedef struct {
    uint32_t counter;
    uint32_t timeStamp;
    float setPoint;
    float input;
} logger_t;

static void logData(logger_t *logger) {
    UARTprintf1("%u,%u,%d.%02u,%d.%02u\n",
                                        logger->counter,
                                        logger->timeStamp,
                                        (int16_t)logger->setPoint, (uint16_t)(abs(logger->setPoint * 100.0f) % 100),
                                        (int16_t)logger->input, (uint16_t)(abs(logger->input * 100.0f) % 100));
    UARTFlushTx1(false);
}

float logStateMachine(bool active, float setPoint, float input, float step1, float step2, uint32_t interval, uint32_t now) {
    static uint8_t state;

    if (active) {
        static logger_t logger;
        static uint32_t startTime, stateTimer;

        switch (state) {
            case 0:
                startTime = stateTimer = now; // Set initial value
                logger.counter = 0; // Reset counter
                setPoint = step1;
                state = 1;
                break;
            case 1:
                setPoint = step1;
                if ((int32_t)(now - stateTimer) >= interval) {
                    stateTimer = now;
                    state = 2;
                }
                break;
            case 2:
                setPoint = step2;
                if ((int32_t)(now - stateTimer) >= interval) {
                    stateTimer = now;
                    state = 3;
                }
                break;
            case 3:
                setPoint = step1;
                if ((int32_t)(now - stateTimer) >= interval)
                    state = 4;
                break;
            case 4:
                // Do nothing!
                break;
            default:
                break;
        }

        if (state < 4) { // Log data if state machine is running
            GPIOPinWrite(GPIO_LED_BASE, GPIO_BLUE_LED, GPIO_BLUE_LED); // Turn on blue LED

            logger.counter++;
            logger.timeStamp = now - startTime;
            logger.setPoint = setPoint;
            logger.input = input;

            logData(&logger);
        } else
            GPIOPinWrite(GPIO_LED_BASE, GPIO_BLUE_LED, 0); // Turn off blue LED
    } else {
        state = 0;
        GPIOPinWrite(GPIO_LED_BASE, GPIO_BLUE_LED, 0); // Turn off blue LED
    }

    return setPoint;
}

#endif
