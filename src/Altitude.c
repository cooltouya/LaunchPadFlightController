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

// Inspired by: https://github.com/cleanflight/cleanflight/blob/master/src/main/sensors/barometer.c and https://github.com/cleanflight/cleanflight/blob/master/src/main/flight/altitudehold.c
// Note that I reused the filtered value from the attitude estimation instead of filtering the LPF accelerometer data like Cleanflight does

#if USE_SONAR || USE_BARO

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "Altitude.h"
#include "BMP180.h"
#include "Buzzer.h"
#include "MPU6500.h"
#include "PID.h"
#include "PPM.h"
#include "RX.h"
#include "Sonar.h"

#include "utils/uartstdio.h" // Add "UART_BUFFERED" to preprocessor

// TODO: Use sonar to estimate baro offset, for smooth transaction

#if USE_BARO
static bmp180_t bmp180; // Barometer readings
#endif

static bool altHoldActive;

void initAltitudeHold(void) {
    initSonar();
#if USE_BARO
    intBMP180(&bmp180);
#endif

#if UART_DEBUG && USE_BARO
    UARTprintf("Barometer values: %d\t%d\t%d\n", bmp180.pressure, bmp180.temperature, (int32_t)bmp180.groundAltitude);
#endif
}

void getAltitudeHold(void) {
#if USE_SONAR
    if (triggerSonar()) { // Trigger sonar
#if 0 // Set to 1 to debug sonar sensor
    #if USE_BARO
        int16_t distance = getSonarDistance(&angle, &bmp180);
    #else
        int16_t distance = getSonarDistance(&angle);
    #endif
        UARTprintf("Distance: %d\n", distance);
#endif
    }
#endif

#if USE_BARO
    if (getBMP180Data(&bmp180)) {
#if 0
        UARTprintf("BMP180: %d %d %d\n", bmp180.pressure, bmp180.temperature, (int32_t)(bmp180.absoluteAltitude - bmp180.groundAltitude));
#endif
    }
#endif
}

// TODO: Use MPU-6500 values for barometer altitude hold code
float updateAltitudeHold(angle_t *angle, mpu6500_t *mpu6500, uint8_t maxAngleInclinationSonar, float throttle, float dt) {
    static const float throttle_noise_lpf = 1000.0f; // TODO: Set via app
    static float altHoldThrottle; // Low pass filtered throttle input
    static float altHoldInitialThrottle; // Throttle when altitude hold was activated
    static int16_t altHoldSetPoint; // Altitude hold set point

#if USE_BARO
    int16_t distance = getSonarDistance(angle, &bmp180, maxAngleInclinationSonar);
#else
    int16_t distance = getSonarDistance(angle, maxAngleInclinationSonar);
#endif

    // TODO: Use barometer when it exceeds 3m
    if (distance >= 0) { // Make sure the distance is valid
        if (!altHoldActive) { // We just went from deactivated to active
            altHoldActive = true;
            resetPIDAltHold();
            altHoldThrottle = throttle; // Set low pass filtered throttle value
            altHoldSetPoint = distance > 1500 ? 1500 : distance; // Set new altitude hold set point - limit to 1.5m
            altHoldInitialThrottle = throttle; // Save current throttle
            if (altHoldInitialThrottle < CHANNEL_MIN_CHECK) { // If throttle is very low, just set an initial value, so it still works
                // TODO: Don't hardcode these values
                altHoldSetPoint = 1000; // Set to 1m
                altHoldInitialThrottle = -30.0f; // Set the throttle value to where is approximately hovers
            }
        }

        altHoldThrottle = altHoldThrottle * (1.0f - (1.0f / throttle_noise_lpf)) + throttle * (1.0f / throttle_noise_lpf); // LPF throttle input

        float setPoint;
        if (altHoldThrottle < altHoldInitialThrottle)
            setPoint = mapf(altHoldThrottle, -100.0f, altHoldInitialThrottle, 50.0f, altHoldSetPoint); // Limit minimum value to 5cm
        else
            setPoint = mapf(altHoldThrottle, altHoldInitialThrottle, 100.0f, altHoldSetPoint, 1500.0f); // Limit maximum altitude to 1.5m which is in practice the limit of the sonar

        float altHoldOut = updatePID(&pidAltHold, setPoint, distance, dt);
        // Throttle value is set to throttle when altitude hold were first activated plus output from PID controller
        // Set minimum to -90, so the motors are never completely shut off
        throttle = constrain(altHoldInitialThrottle + altHoldOut, -90.0f, 100.0f);
        /*UARTprintf("%u %d %d %d - %d %d %d %d\n", altHoldActive, (int32_t)altHoldThrottle, (int32_t)altHoldInitialThrottle, altHoldSetPoint,     (int32_t)setPoint, distance, (int32_t)altHoldOut, (int32_t)throttle);
        UARTFlushTx(false);*/
    } else
        buzzer(true); // Turn on buzzer in case sonar sensor return an error

    return throttle;
}

void resetAltitudeHold(void) {
    altHoldActive = false;
}

#endif // USE_SONAR || USE_BARO
