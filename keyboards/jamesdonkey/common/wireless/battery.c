/* Copyright 2023 @ lokher (https://www.lemokey.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "quantum.h"
#include "wireless.h"
#include "battery.h"
#include "transport.h"
#include "lkbt51.h"
#include "lpm.h"
#include "indicator.h"
#include "rtc_timer.h"
#include "analog.h"

#define BATTERY_EMPTY_COUNT 10
#define CRITICAL_LOW_COUNT 20

/* Battery voltage resistive voltage divider setting of MCU */
#ifndef RVD_R1
#    define RVD_R1 10 // Upper side resitor value (uint: KΩ)
#endif
#ifndef RVD_R2
#    define RVD_R2 10 // Lower side resitor value (uint: KΩ)
#endif

/* Battery voltage resistive voltage divider setting of Bluetooth */
#ifndef LKBT51_RVD_R1
#    define LKBT51_RVD_R1 560
#endif
#ifndef LKBT51_RVD_R2
#    define LKBT51_RVD_R2 499
#endif

#ifndef VOLTAGE_TRIM_LED_MATRIX
#    define VOLTAGE_TRIM_LED_MATRIX 30
#endif

#ifndef VOLTAGE_TRIM_RGB_MATRIX
#    define VOLTAGE_TRIM_RGB_MATRIX 60
#endif

#if defined(LED_MATRIX_ENABLE) || defined(RGB_MATRIX_ENABLE)
extern uint8_t g_pwm_buffer[DRIVER_COUNT][192];
#endif

static uint32_t bat_monitor_timer_buffer = 0;
static uint16_t voltage                  = FULL_VOLTAGE_VALUE;
static uint8_t  bat_empty                = 0;
static uint8_t  critical_low             = 0;
static uint8_t  bat_state;
static uint8_t  power_on_sample = 0;
static uint8_t  reported_percentage = 0xFF;

#ifdef SIDE_LED_VDD
extern void side_light_power_off(void);
bool side_empty_voltage_flag=false;
#endif

void battery_init(void) {
    bat_state = BAT_NOT_CHARGING;
#if defined(BAT_CHARGING_PIN)
#    if (BAT_CHARGING_LEVEL == 0)
    palSetLineMode(BAT_CHARGING_PIN, PAL_MODE_INPUT_PULLUP);
#    else
    palSetLineMode(BAT_CHARGING_PIN, PAL_MODE_INPUT_PULLDOWN);
#    endif
#endif

#ifdef BAT_ADC_ENABLE_PIN
    palSetLineMode(BAT_ADC_ENABLE_PIN, PAL_MODE_OUTPUT_PUSHPULL);
    writePin(BAT_ADC_ENABLE_PIN, 1);
#endif
#ifdef BAT_ADC_PIN
    palSetLineMode(BAT_ADC_PIN, PAL_MODE_INPUT_ANALOG);
#endif
}

void battery_stop(void) {
#if (HAL_USE_ADC)
#    ifdef BAT_ADC_ENABLE_PIN
    writePin(BAT_ADC_ENABLE_PIN, 0);
#    endif
#    ifdef BAT_ADC_PIN
    palSetLineMode(BAT_ADC_PIN, PAL_MODE_INPUT_ANALOG);
    analog_stop(BAT_ADC_PIN);
#    endif
#endif
}

__attribute__((weak)) void battery_measure(void) {
    lkbt51_read_state_reg(0x05, 0x02);
}

/* Calculate the voltage */
__attribute__((weak)) void battery_calculate_voltage(bool vol_src_bt, uint16_t value) {
    uint16_t voltage;

    if (vol_src_bt)
        voltage = ((uint32_t)value) * (LKBT51_RVD_R1 + LKBT51_RVD_R2) / LKBT51_RVD_R2;
    else
        voltage = (uint32_t)value * 3300 / 1024 * (RVD_R1 + RVD_R2) / RVD_R2;

#ifdef LED_MATRIX_ENABLE
    if (led_matrix_is_enabled()) {
        uint32_t totalBuf = 0;

        for (uint8_t i = 0; i < DRIVER_COUNT; i++)
            for (uint8_t j = 0; j < 192; j++)
                totalBuf += g_pwm_buffer[i][j];
        /* We assumpt it is linear relationship*/
        voltage += (VOLTAGE_TRIM_LED_MATRIX * totalBuf / LED_MATRIX_LED_COUNT / 255);
    }
#endif
#ifdef RGB_MATRIX_ENABLE
    if (rgb_matrix_is_enabled()) {
        uint32_t totalBuf = 0;

        for (uint8_t i = 0; i < DRIVER_COUNT; i++)
            for (uint8_t j = 0; j < 192; j++)
                totalBuf += g_pwm_buffer[i][j];
        /* We assumpt it is linear relationship*/
        uint32_t compensation = VOLTAGE_TRIM_RGB_MATRIX * totalBuf / RGB_MATRIX_LED_COUNT / 255 / 3;

        voltage += compensation;
    }
#endif

    battery_set_voltage(voltage);
}

/* The pack is sampled under load, and the current burst of a radio transmission
   sags it by well over 100mV. With 7.5mV per percentage point that is enough to
   swing the reported level by twenty points between two consecutive readings, so
   take the median of the last few samples: unlike an average it discards isolated
   dips outright instead of letting them pull the result down. */
#define VOLTAGE_MEDIAN_SAMPLES 5
static uint16_t voltage_samples[VOLTAGE_MEDIAN_SAMPLES];
static uint8_t  voltage_sample_count = 0;
static uint8_t  voltage_sample_next  = 0;

void battery_set_voltage(uint16_t value) {
    voltage_samples[voltage_sample_next] = value;
    voltage_sample_next                  = (voltage_sample_next + 1) % VOLTAGE_MEDIAN_SAMPLES;
    if (voltage_sample_count < VOLTAGE_MEDIAN_SAMPLES) voltage_sample_count++;

    uint16_t sorted[VOLTAGE_MEDIAN_SAMPLES];
    memcpy(sorted, voltage_samples, voltage_sample_count * sizeof(uint16_t));

    for (uint8_t i = 1; i < voltage_sample_count; i++) {
        uint16_t key = sorted[i];
        int8_t   j   = i - 1;
        while (j >= 0 && sorted[j] > key) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }

    voltage = sorted[voltage_sample_count / 2];
}

uint16_t battery_get_voltage(void) {
    return voltage;
}

uint8_t battery_get_percentage(void) {
    if (voltage > FULL_VOLTAGE_VALUE) return 100;

    if (voltage > EMPTY_VOLTAGE_VALUE) {
        return ((uint32_t)voltage - EMPTY_VOLTAGE_VALUE) * 80 / (FULL_VOLTAGE_VALUE - EMPTY_VOLTAGE_VALUE) + 20;
    }

    if (voltage > SHUTDOWN_VOLTAGE_VALUE) {
        return ((uint32_t)voltage - SHUTDOWN_VOLTAGE_VALUE) * 20 / (EMPTY_VOLTAGE_VALUE - SHUTDOWN_VOLTAGE_VALUE);
    } else
        return 0;
}

bool battery_is_empty(void) {
    return bat_empty > BATTERY_EMPTY_COUNT;
}

bool battery_is_critical_low(void) {
    return critical_low > CRITICAL_LOW_COUNT;
}

void battery_check_empty(void) {
    if (voltage < EMPTY_VOLTAGE_VALUE) {
        if (bat_empty <= BATTERY_EMPTY_COUNT) {
            if (++bat_empty > BATTERY_EMPTY_COUNT) {
                indicator_battery_low_enable(true);
                power_on_sample = VOLTAGE_POWER_ON_MEASURE_COUNT;
#ifdef SIDE_LED_VDD
                side_light_power_off();
                side_empty_voltage_flag = true;
#endif
            }
        }
    } else if (bat_empty <= BATTERY_EMPTY_COUNT) {
        /* Require consecutive low readings, as battery_check_critical_low() does.
           Without this reset the counter accumulated isolated voltage sags (e.g. the
           current burst of a bluetooth transmission) and eventually latched on a
           perfectly healthy battery. */
        bat_empty = 0;
    }
}

void battery_check_critical_low(void) {
    if (voltage < SHUTDOWN_VOLTAGE_VALUE) {
        if (critical_low <= CRITICAL_LOW_COUNT) {
            if (++critical_low > CRITICAL_LOW_COUNT) wireless_low_battery_shutdown();
        }
    } else if (critical_low <= CRITICAL_LOW_COUNT) {
        critical_low = 0;
    }
}

bool battery_power_on_sample(void) {
    return power_on_sample < VOLTAGE_POWER_ON_MEASURE_COUNT;
}

void battery_task(void) {
    uint32_t t = rtc_timer_elapsed_ms(bat_monitor_timer_buffer);
    if ((get_transport() & TRANSPORT_WIRELESS) && (wireless_get_state() == WT_CONNECTED || battery_power_on_sample())) {
#if defined(BAT_CHARGING_PIN)
        if (usb_power_connected() && t > VOLTAGE_MEASURE_INTERVAL) {
            if (readPin(BAT_CHARGING_PIN) == BAT_CHARGING_LEVEL)
                lkbt51_update_bat_state(BAT_CHARGING);
            else
                lkbt51_update_bat_state(BAT_FULL_CHARGED);
        }
#endif

        if ((battery_power_on_sample()
#if defined(LED_MATRIX_ENABLE) || defined(RGB_MATRIX_ENABLE)
             && !indicator_is_enabled()
#endif
             && t > BACKLIGHT_OFF_VOLTAGE_MEASURE_INTERVAL) ||
            t > VOLTAGE_MEASURE_INTERVAL) {

            battery_check_empty();
            battery_check_critical_low();

            bat_monitor_timer_buffer = rtc_timer_read_ms();
            if (bat_monitor_timer_buffer > RTC_MAX_TIME) {
                bat_monitor_timer_buffer = 0;
                rtc_timer_clear();
            }

            battery_measure();
            if (power_on_sample < VOLTAGE_POWER_ON_MEASURE_COUNT) power_on_sample++;
        }

        /* The level used to be pushed only once, from wireless_enter_connected(), so
           the host kept displaying whatever it was at the moment of connection while
           the measurements below carried on unreported. Send an update whenever the
           computed percentage actually changes. */
        if (wireless_get_state() == WT_CONNECTED) {
            uint8_t percentage = battery_get_percentage();
            if (percentage != reported_percentage) {
                reported_percentage = percentage;
                wireless_update_bat_level(percentage);
            }
        }
    }

    if ((bat_empty || critical_low) && usb_power_connected()) {
        bat_empty    = false;
        critical_low = false;
        indicator_battery_low_enable(false);
    }
}
