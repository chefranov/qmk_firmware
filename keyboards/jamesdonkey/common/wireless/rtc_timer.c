/* Copyright 2023 @ lokher (https://www.keychron.com)
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

#include "hal.h"
#include "rtc_timer.h"

#if (HAL_USE_RTC)

void rtc_timer_init(void) {
    rtc_timer_clear();
}

void rtc_timer_clear(void) {
    RTCDateTime tm = {0, 0, 0, 0, 0, 0};
    rtcSetTime(&RTCD1, &tm);
}

uint32_t rtc_timer_read_ms(void) {
    RTCDateTime tm;
    rtcGetTime(&RTCD1, &tm);

    return tm.millisecond;
}

uint32_t rtc_timer_elapsed_ms(uint32_t last) {
    return TIMER_DIFF_32(rtc_timer_read_ms(), last);
}

#else

/* No RTC available: fall back to the system tick. Note that the tick is halted
   while the MCU is in STOP mode, so timeouts that span sleep will under-count. */

void rtc_timer_init(void) {}

void rtc_timer_clear(void) {}

uint32_t rtc_timer_read_ms(void) {
    return timer_read32();
}

uint32_t rtc_timer_elapsed_ms(uint32_t last) {
    return timer_elapsed32(last);
}

#endif
