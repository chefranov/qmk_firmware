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
#ifdef LK_WIRELESS_ENABLE
#    include "lkbt51.h"
#    include "wireless.h"
#    include "indicator.h"
#    include "transport.h"
#    include "battery.h"
#    include "bat_level_animation.h"
#    include "lpm.h"
#    include "wireless_common.h"
#    include "task.h"
#endif
#include "common.h"
#include "config.h"

bool firstDisconnect = true;

static uint32_t pairing_key_timer;
static uint8_t  host_idx = 0;

#ifdef USB_INDICATION_LED_INDEX
extern bool usb_host_indicator_enable;
#endif

bool process_record_wireless_common(uint16_t keycode, keyrecord_t *record) {
    static uint8_t host_idx;

    switch (keycode) {
        case BT_HST1 ... BT_HST3:
#ifdef BT_MODE_SELECT_PIN
            if (get_transport() == TRANSPORT_BLUETOOTH)
#endif
            {
                if (record->event.pressed) {
#ifdef TRANSPORT_SOFT_SWITCH_ENABLE
                    if (eeprom_read_transport() != TRANSPORT_BLUETOOTH) {
                        set_transport(TRANSPORT_BLUETOOTH);
#    if (EECONFIG_KB_DATA_SIZE > 0)
                        eeprom_update_transport(TRANSPORT_BLUETOOTH);
#    endif
                    }
#endif
                    host_idx          = keycode - BT_HST1 + 1;
                    pairing_key_timer = timer_read32();
                    wireless_connect_ex(host_idx, 0);
                } else {
                    host_idx          = 0;
                    pairing_key_timer = 0;
                }
            }
            break;

        case P2P4G:
#ifdef P2P4_MODE_SELECT_PIN
            if (get_transport() == TRANSPORT_P2P4)
#endif
            {
                if (record->event.pressed) {
#ifdef TRANSPORT_SOFT_SWITCH_ENABLE
                    if (eeprom_read_transport() != TRANSPORT_P2P4) {
                        set_transport(TRANSPORT_P2P4);
#    if (EECONFIG_KB_DATA_SIZE > 0)
                        eeprom_update_transport(TRANSPORT_P2P4);
#    endif
                    }
#endif
                    host_idx          = P24G_INDEX;
                    pairing_key_timer = timer_read32();
                } else {
                    host_idx          = 0;
                    pairing_key_timer = 0;
                }
            }
            break;

#ifdef TRANSPORT_SOFT_SWITCH_ENABLE
        case MD_USB:
            if (record->event.pressed) {
                if (usb_power_connected()) {
                    if (eeprom_read_transport() != TRANSPORT_USB) {
                        set_transport(TRANSPORT_USB);
#    if (EECONFIG_KB_DATA_SIZE > 0)
                        eeprom_update_transport(TRANSPORT_USB);
#    endif
                    }
                }
            }
            break;
#endif
        case BAT_LVL:
            if ((get_transport() & TRANSPORT_WIRELESS) && !usb_power_connected()) {
                bat_level_animiation_start(battery_get_percentage());
            }
            break;

        default:
#ifdef USB_INDICATION_LED_INDEX
            if (get_transport() == TRANSPORT_USB && USBD1.state != USB_ACTIVE && keycode < QK_BASIC_MAX) {
                indicator_set(WT_RECONNECTING, USB_HOST_INDEX);
            }
#endif
            break;
    }

    return true;
}

void lkbt51_param_init(void) {
    /* Set bluetooth device name */
    lkbt51_set_local_name(PRODUCT);
    wait_ms(3);
    // clang-format off
    /* Set bluetooth parameters */
    module_param_t param = {.event_mode             = 0x02,
                            .connected_idle_timeout = 7200,
                            .pairing_timeout        = 180,
                            .pairing_mode           = 0,
                            .reconnect_timeout      = 5,
                            .report_rate            = 90,
                            .vendor_id_source       = 1,
                            .verndor_id             = VENDOR_ID,
                            .product_id             = PRODUCT_ID};
    // clang-format on
    lkbt51_set_param(&param);
}

void wireless_enter_reset_kb(uint8_t reason) {
    lkbt51_param_init();
}

void wireless_enter_disconnected_kb(uint8_t host_idx, uint8_t reason) {

    /* CKBT51 bluetooth module boot time is slower, it enters disconnected after boot,
       so we place initialization here. The one-shot flag alone identifies that first
       post-boot disconnect; the tick-based window this used to carry was unreliable,
       because the system tick is halted during STOP mode and so kept the window open
       for minutes of wall time, letting a later disconnect re-run the init. */
    if (firstDisconnect) {
        lkbt51_param_init();
        if (get_transport() == TRANSPORT_BLUETOOTH) wireless_connect();
        firstDisconnect = false;
    }
}

void wireless_common_task(void) {
    if (pairing_key_timer) {
        if (timer_elapsed32(pairing_key_timer) > 2000) {
            pairing_key_timer = 0;
            wireless_pairing_ex(host_idx, NULL);
        }
    }
}

extern bool wireless_pre_task_kb(void);
void wireless_pre_task(void) {
    if (!wireless_pre_task_kb()) return;
#ifdef TRANSPORT_SOFT_SWITCH_ENABLE
    if (get_transport() == 0) {
        uint8_t mode = eeprom_read_transport();
        if (mode == 0) {
            mode = TRANSPORT_USB;
            eeprom_update_transport(mode);
        }

        set_transport(mode);
    }
#else
    static uint8_t  dip_switch_state = 0;
    static uint32_t time = 0;

    if (time == 0) {
        uint8_t pins_state = (readPin(BT_MODE_SELECT_PIN) << 1)
#    ifdef P2P4_MODE_SELECT_PIN
                             | readPin(P2P4_MODE_SELECT_PIN)
#    endif
            ;

        if (pins_state != dip_switch_state) {
            dip_switch_state = pins_state;
            time = timer_read32();
        }
    }

    if ((time && timer_elapsed32(time) > 100) || get_transport() == TRANSPORT_NONE) {
        uint8_t pins_state = (readPin(BT_MODE_SELECT_PIN) << 1)
#    ifdef P2P4_MODE_SELECT_PIN
                             | readPin(P2P4_MODE_SELECT_PIN)
#    endif
            ;

        if (pins_state == dip_switch_state) {
            time = 0;

            switch (dip_switch_state) {
                case 0x01:
                    set_transport(TRANSPORT_BLUETOOTH);
                    break;
                case 0x02:
#    ifdef P2P4_MODE_SELECT_PIN
                    set_transport(TRANSPORT_P2P4);
#    endif
                    break;
                case 0x03:
#    ifdef P2P4_MODE_SELECT_PIN
                    set_transport(TRANSPORT_USB);
#    endif
                    break;
                default:
                    break;
            }
        } else {
            dip_switch_state = pins_state;
            time = timer_read32();
        }
    }
#endif
}
