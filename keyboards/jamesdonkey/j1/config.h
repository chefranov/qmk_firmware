/* Copyright 2024 @ James Donkey
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

#pragma once

/* I2C Driver Configuration */
#define I2C1_SCL_PIN B8
#define I2C1_SDA_PIN B9
#define I2C1_CLOCK_SPEED 400000
#define I2C1_DUTY_CYCLE FAST_DUTY_CYCLE_2
#define I2C1_OPMODE OPMODE_I2C

/* EEPROM Driver Configuration */
#define EXTERNAL_EEPROM_BYTE_COUNT 2048
#define EXTERNAL_EEPROM_PAGE_SIZE  32
#define EXTERNAL_EEPROM_WRITE_TIME 3
#define EXTERNAL_EEPROM_I2C_BASE_ADDRESS 0b10100010
#define KEYCODE_BUFFER_ENABLE

#ifdef RGB_MATRIX_ENABLE
/* RGB Matrix driver configuration */
#    define DRIVER_COUNT 2
#    define DRIVER_CS_PINS \
        { C9, A8 }
#    define RGB_MATRIX_LED_COUNT 82
#    define SNLED23751_SPI_DIVISOR 16
#    define LED_DRIVER_SHUTDOWN_PIN B7

/* Scan phase of led driver set as MSKPHASE_9CHANNEL(defined as 0x03 in snled27351.h) */
#    define PHASE_CHANNEL MSKPHASE_9CHANNEL

/* Set LED driver current */
#    define SNLED27351_CURRENT_TUNE \
        { 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40 }

/* Set to infinit, which is use in USB mode by default */
#    define RGB_MATRIX_TIMEOUT RGB_MATRIX_TIMEOUT_INFINITE
/* Allow shutdown of led driver to save power */
#    define RGB_MATRIX_DRIVER_SHUTDOWN_ENABLE
/* Turn off backlight on low brightness to save power */
#    define RGB_MATRIX_BRIGHTNESS_TURN_OFF_VAL 48

/* Indications */
#    define WINLOCK_LED_LIST \
        { 73 }
#    define WIN_LOCK_HOLD_TIME 3000

#    define RGB_MATRIX_KEYPRESSES
#    define RGB_MATRIX_FRAMEBUFFER_EFFECTS
#endif

#ifdef LK_WIRELESS_ENABLE
/* Hardware configuration */
#    define P2P4_MODE_SELECT_PIN A9
#    define BT_MODE_SELECT_PIN A10

#    define LKBT51_RESET_PIN C4
#    define LKBT51_INT_INPUT_PIN B1
#    define BLUETOOTH_INT_OUTPUT_PIN A4

#    define USB_POWER_SENSE_PIN B0
#    define USB_POWER_CONNECTED_LEVEL 0

#    define BAT_CHARGING_PIN C10
#    define BAT_CHARGING_LEVEL 0

#    define BAT_LOW_LED_PIN A15
#    define BAT_LOW_LED_PIN_ON_STATE 1

#    define DP_PULLUP_CONTROL_PIN C11

#    define BT_HOST_DEVICES_COUNT 3

#    define BT_INDICATION_LED_PIN_LIST \
        { B13, B13, B13 }

#    define BT_INDICATION_LED_ON_STATE 0

#    define P24G_INDICATION_LED_PIN B12
#    define P24G_INDICATION_LED_PIN_ON_STATE 0

#    if defined(RGB_MATRIX_ENABLE)
#        define BT_INDICATION_LED_LIST \
            { 16, 17, 18 }

#        define BAT_LEVEL_LED_LIST \
            { 16, 17, 18, 19, 20, 21, 22, 23, 24, 25 }

#    define  P24G_INDICATION_LED_INDEX 19

/* Reinit LED driver on tranport changed */
#        define REINIT_LED_DRIVER 1
#    endif

/* Keep USB connection in blueooth mode */
#    define KEEP_USB_CONNECTION_IN_WIRELESS_MODE

/* STOP mode does not resume reliably on this board: once it has really slept -
   which needs the cable unplugged, since KEEP_USB_CONNECTION_IN_WIRELESS_MODE
   blocks deep sleep while USB power is present - it never wakes again, from the
   key matrix or from any other source. Moving the low-power transition code into
   RAM was not enough. Left disabled until it can be debugged properly. */
#    define LK_DISABLE_STOP_MODE

/* Enable bluetooth NKRO */
#    define WIRELESS_NKRO_ENABLE
#endif

#if defined(RGB_MATRIX_ENABLE) || defined(LK_WIRELESS_ENABLE)
/* SPI configuration */
#    define SPI_DRIVER SPIDQ
#    define SPI_SCK_PIN A5
#    define SPI_MISO_PIN A6
#    define SPI_MOSI_PIN A7
#endif

/* Factory test keys */
#define FN_KEY_1 MO(1)
#define FN_KEY_2 MO(3)

#define FN_BL_TRIG_KEY KC_END

#define MATRIX_IO_DELAY 10
