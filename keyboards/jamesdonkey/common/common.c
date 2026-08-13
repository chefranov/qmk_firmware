/* Copyright 2022 @ Keychron (https://www.keychron.com)
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

#include QMK_KEYBOARD_H
#include "common.h"
#include "raw_hid.h"
#include "version.h"
#ifdef VIA_ENABLE
#    include "via.h"
#endif
#include "eeconfig.h"
#include "common.h"
#ifdef FACTORY_TEST_ENABLE
#    include "factory_test.h"
#endif

#ifdef LK_WIRELESS_ENABLE
#    include "lkbt51.h"
#    include "wireless.h"
#    include "battery.h"
#endif

#if defined(LK_WIRELESS_ENABLE) && defined(RAW_ENABLE)
/* Override of the core's weak raw_hid_send(). Launcher/VIA traffic also arrives over
   the wireless link when the keyboard is reached through the 2.4GHz receiver, and the
   reply has to leave by the same route - the USB endpoint is not connected then. */
void raw_hid_send(uint8_t *data, uint8_t length) {
    if (raw_hid_get_src() == RAW_HID_SRC_WIRELESS) {
        wireless_send_raw_hid(data, length);
        return;
    }

    usb_raw_hid_send(data, length);
}
#endif

#ifdef LED_MATRIX_ENABLE
#    include "led_matrix.h"
#endif

#include "side_rgb/side_rgb.h"

bool     is_siri_active = false;
uint32_t siri_timer     = 0;

static uint8_t mac_keycode[4] = {
    KC_LOPT,
    KC_ROPT,
    KC_LCMD,
    KC_RCMD,
};

#if defined(WIN_LOCK_HOLD_TIME)
static uint32_t winlock_timer = 0;
#endif
// extern keymap_config_t keymap_config;

// clang-format off
static key_combination_t key_comb_list[] = {
    {2, {KC_LWIN, KC_TAB}},
    {2, {KC_LWIN, KC_E}},
    {3, {KC_LSFT, KC_LCMD, KC_4}},
    {2, {KC_LWIN, KC_C}},
#ifdef WIN_LOCK_SCREEN_ENABLE
    {2, {KC_LWIN, KC_L}},
#endif
#ifdef MAC_LOCK_SCREEN_ENABLE
    {3, {KC_LCTL, KC_LCMD, KC_Q}},
#endif
};
// clang-format on

#ifdef OS_TOGGLE_LED_INDEX
static uint32_t os_toggle_timer            = 0;
bool            os_toggle_indicator_enable = false;
#endif

void gui_toggle(void) {
    keymap_config.no_gui = !keymap_config.no_gui;
    eeconfig_update_keymap(keymap_config.raw);
    led_update_kb(host_keyboard_led_state());
}

bool process_record_common(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case KC_LOPTN:
        case KC_ROPTN:
        case KC_LCMMD:
        case KC_RCMMD:
            if (record->event.pressed) {
                register_code(mac_keycode[keycode - KC_LOPTN]);
            } else {
                unregister_code(mac_keycode[keycode - KC_LOPTN]);
            }
            return false; // Skip all further processing of this key
        case KC_MCTRL:
            if (record->event.pressed) {
                register_code(KC_MISSION_CONTROL);
            } else {
                unregister_code(KC_MISSION_CONTROL);
            }
            return false; // Skip all further processing of this key
        case KC_LNPAD:
            if (record->event.pressed) {
                register_code(KC_LAUNCHPAD);
            } else {
                unregister_code(KC_LAUNCHPAD);
            }
            return false; // Skip all further processing of this key
        case KC_TASK:
        case KC_FILE:
        case KC_SNAP:
        case KC_CTANA:
#ifdef WIN_LOCK_SCREEN_ENABLE
        case KC_WLCK:
#endif
#ifdef MAC_LOCK_SCREEN_ENABLE
        case KC_MLCK:
#endif
            if (record->event.pressed) {
                for (uint8_t i = 0; i < key_comb_list[keycode - KC_TASK].len; i++) {
                    register_code(key_comb_list[keycode - KC_TASK].keycode[i]);
                }
            } else {
                for (uint8_t i = 0; i < key_comb_list[keycode - KC_TASK].len; i++) {
                    unregister_code(key_comb_list[keycode - KC_TASK].keycode[i]);
                }
            }
            return false; // Skip all further processing of this key
        case KC_SIRI:
            if (record->event.pressed) {
                if (!is_siri_active) {
                    is_siri_active = true;
                    register_code(KC_LCMD);
                    register_code(KC_SPACE);
                }
                siri_timer = timer_read32();
            } else {
                // Do something else when release
            }
            return false; // Skip all further processing of this key
        case QK_MAGIC_TOGGLE_NKRO:
            if (record->event.pressed) {
                // keymap_config.raw = eeconfig_read_keymap();

                clear_keyboard(); // clear first buffer to prevent stuck keys
                keymap_config.nkro = !keymap_config.nkro;
#ifdef NKRO_TOGGLE_INDICATION_ENABLE
                factory_indication_start();
#endif
                eeconfig_update_keymap(keymap_config.raw);
                clear_keyboard(); // clear to prevent stuck keys
            }
            return false;

#if defined(WIN_LOCK_HOLD_TIME) || defined(WIN_LOCK_LED_PIN) || defined(WINLOCK_LED_LIST)
        case GU_TOGG:
#    if defined(WIN_LOCK_HOLD_TIME)
            if (record->event.pressed) {
                winlock_timer = timer_read32();
            } else {
                winlock_timer = 0;
            }
#    else
            if (record->event.pressed) gui_toggle();
#    endif
            return false;
#endif
#ifdef LED_MATRIX_ENABLE
        case BL_SPI:
            led_matrix_increase_speed();
            break;
        case BL_SPD:
            led_matrix_decrease_speed();
            break;
#endif

        default:
            return true; // Process all other keycodes normally
    }
    return true;
}

void common_task(void) {
    if (is_siri_active && timer_elapsed32(siri_timer) > 500) {
        unregister_code(KC_LCMD);
        unregister_code(KC_SPACE);
        is_siri_active = false;
        siri_timer     = 0;
    }

#ifdef OS_TOGGLE_LED_INDEX
    if (os_toggle_timer && timer_elapsed32(os_toggle_timer) > 3000) {
        os_toggle_timer = 0;
        os_toggle();
    }
#endif

#if defined(WIN_LOCK_HOLD_TIME)
    if (winlock_timer) {
        if (keymap_config.no_gui) {
            winlock_timer = 0;
            gui_toggle();
        } else if (timer_elapsed32(winlock_timer) > WIN_LOCK_HOLD_TIME) {
            factory_indication_start();
            winlock_timer = 0;
            gui_toggle();
        }
    }
#endif
}

#ifdef ENCODER_ENABLE
static void encoder0_pad_cb(void *param) {
    (void)param;
    encoder_inerrupt_read(0);
}

void encoder_cb_init(void) {
    pin_t encoders_pad_a[] = ENCODERS_PAD_A;
    pin_t encoders_pad_b[] = ENCODERS_PAD_B;
    palEnableLineEvent(encoders_pad_a[0], PAL_EVENT_MODE_BOTH_EDGES);
    palEnableLineEvent(encoders_pad_b[0], PAL_EVENT_MODE_BOTH_EDGES);
    palSetLineCallback(encoders_pad_a[0], encoder0_pad_cb, NULL);
    palSetLineCallback(encoders_pad_b[0], encoder0_pad_cb, NULL);
}
#endif

#define PROTOCOL_VERSION 0x02

enum {
    kc_get_protocol_version = 0xA0,
    kc_get_firmware_version = 0xA1,
    kc_get_support_feature  = 0xA2,
    kc_get_default_layer    = 0xA3,
    kc_get_battery_level    = 0xA4,
};

enum {
    FEATURE_DEFAULT_LAYER = 0x01 << 0,
    FEATURE_BLUETOOTH     = 0x01 << 1,
    FEATURE_P2P4G         = 0x01 << 2,
    FEATURE_ANALOG_MATRIX = 0x01 << 3,
};

void get_support_feature(uint8_t *data) {
    data[1] = FEATURE_DEFAULT_LAYER
#ifdef KC_BLUETOOTH_ENABLE
              | FEATURE_BLUETOOTH
#endif
#ifdef LK_WIRELESS_ENABLE
              | FEATURE_BLUETOOTH | FEATURE_P2P4G
#endif
#ifdef ANANLOG_MATRIX
              | FEATURE_ANALOG_MATRIX
#endif
        ;
}

bool via_command_kb(uint8_t *data, uint8_t length) {
    switch (data[0]) {
#if defined(VIA_ENABLE) && defined(LK_WIRELESS_ENABLE) && defined(RAW_ENABLE)
        case id_get_protocol_version:
            /* Over the cable the host reads VID/PID off the USB descriptor, but a host
               talking through the 2.4GHz receiver sees the receiver's own descriptor and
               has to ask the keyboard who it is. Without this answer Keychron Launcher
               reports "firmware does not support wireless connection" and refuses to
               configure the board over the dongle. Returning false lets via.c fill in
               the protocol version in data[1..2] and send the packet; the identity
               bytes placed here are further along and survive untouched. */
            if (raw_hid_get_src() != RAW_HID_SRC_USB) {
                data[3] = VENDOR_ID >> 8;
                data[4] = VENDOR_ID & 0xFF;
                data[5] = PRODUCT_ID >> 8;
                data[6] = PRODUCT_ID & 0xFF;
                data[7] = DEVICE_VER >> 8;
                data[8] = DEVICE_VER & 0xFF;
            }
            return false;
#endif

        case kc_get_protocol_version:
#if defined(WEB_DRIVER_KEYCHRON)
            data[1] = KC_PROTOCOL_VERSION >> 8;
            data[2] = KC_PROTOCOL_VERSION & 0xFF;
#else
            data[1] = PROTOCOL_VERSION;
#endif
            break;

        case kc_get_firmware_version: {
            uint8_t i = 1;
            data[i++] = 'v';
            if ((DEVICE_VER & 0xF000) != 0) itoa((DEVICE_VER >> 12), (char *)&data[i++], 16);
            itoa((DEVICE_VER >> 8) & 0xF, (char *)&data[i++], 16);
            data[i++] = '.';
            itoa((DEVICE_VER >> 4) & 0xF, (char *)&data[i++], 16);
            data[i++] = '.';
            itoa((DEVICE_VER >> 0) & 0xF, (char *)&data[i++], 16);
            data[i++] = ' ';
            memcpy(&data[i], QMK_BUILDDATE, sizeof(QMK_BUILDDATE));
            i += sizeof(QMK_BUILDDATE);
        } break;

        case kc_get_support_feature:
            get_support_feature(&data[1]);
            break;

        case kc_get_default_layer:
            data[1] = get_highest_layer(default_layer_state | layer_state);
            break;

#ifdef LK_WIRELESS_ENABLE
        case kc_get_battery_level:
            /* The battery level is otherwise only pushed into the wireless module, which
               exposes it through the bluetooth battery service - so a host reached through
               the 2.4GHz receiver has no way to ask for it. Answer the query here as well.
               Ported from Keychron/qmk_firmware#504. */
            data[1] = battery_get_percentage();
            break;
#endif

#ifdef ANANLOG_MATRIX
        case 0xA9:
            analog_matrix_rx(data, length);
            return true;
            break;
#endif
#ifdef LK_WIRELESS_ENABLE
        case 0xAA:
            lkbt51_dfu_rx(data, length);
            return true;
            break;
#endif
#ifdef FACTORY_TEST_ENABLE
        case 0xAB:
            factory_test_rx(data, length);
            return true;
            break;
#endif
        default:
            return false;
    }

    raw_hid_send(data, length);
    return true;
}

#if !defined(VIA_ENABLE)
void raw_hid_receive(uint8_t *data, uint8_t length) {
    via_command_kb(data, length);
}
#endif

__attribute__((weak)) void suspend_power_down_vendor(void) {
    suspend_power_down_user();
}

void suspend_power_down_kb(void) {
#ifdef SIDE_LED_VDD
    side_light_power_off();
#endif
#ifdef WIN_LOCK_LED_PIN
    writePin(WIN_LOCK_LED_PIN, !WIN_LOCK_LED_ON_LEVEL);
#endif
    suspend_power_down_vendor();
    suspend_power_down_user();
}

__attribute__((weak)) void suspend_wakeup_init_vendor(void) {}

void suspend_wakeup_init_kb(void) {
#ifdef SIDE_LED_VDD
    side_light_power_off();
#endif
#ifdef WIN_LOCK_LED_PIN
    if (keymap_config.no_gui) writePin(WIN_LOCK_LED_PIN, !WIN_LOCK_LED_ON_LEVEL);
#endif
    suspend_wakeup_init_vendor();
    suspend_wakeup_init_user();
}
