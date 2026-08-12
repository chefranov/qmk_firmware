
/* Copyright 2021 @ Keychron (https://www.keychron.com)
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
#include "lpm.h"

bool wakeup_from_lpm;
static pm_t power_mode = PM_RUN;

static const uint32_t pre_lp_code[] = {553863175u, 554459777u, 1208378049u, 4026624001u, 688390415u, 554227969u, 3204472833u, 1198571264u, 1073807360u, 1073808388u};
static const uint32_t post_lp_code[] = {553863177u, 554459777u, 1208509121u, 51443856u, 4026550535u, 1745485839u, 3489677954u, 536895496u, 673389632u, 1198578684u, 1073807360u, 536866816u, 1073808388u};

/* NOTE: these blobs are executed straight out of flash, which looks wrong - they
   drive the flash controller across the STOP transition - but copying them into
   SRAM and calling them there was tried and made the board stop waking at all, so
   upstream's arrangement is kept. Do not "fix" without testing sleep on battery
   with the cable unplugged; USB power blocks deep sleep entirely. */
#define PRE_LP() ((void (*)(void))((unsigned int)(pre_lp_code) | 0x01))()
#define POST_LP() ((void (*)(void))((unsigned int)(post_lp_code) | 0x01))()

extern void __early_init(void);
extern void matrix_init_pins(void);

void stop_mode_entry(void);

void lpm_post_enter_low_power(void) {
    /*  USB D+/D- */
    palSetLineMode(A12, PAL_MODE_INPUT_PULLUP); // why PAL_MODE_INPUT_PULLUP
    palSetLineMode(A11, PAL_MODE_INPUT_PULLDOWN);

    palSetLineMode(DP_PULLUP_CONTROL_PIN, PAL_MODE_INPUT_PULLDOWN);
}

void lpm_pre_wakeup(void) {
    /*  USB D+/D- */
    palSetLineMode(A11, PAL_WB32_OTYPE_PUSHPULL | PAL_WB32_OSPEED_HIGH | PAL_WB32_PUPDR_FLOATING | PAL_MODE_ALTERNATE(10U));
    palSetLineMode(A12, PAL_WB32_OTYPE_PUSHPULL | PAL_WB32_OSPEED_HIGH | PAL_WB32_PUPDR_FLOATING | PAL_MODE_ALTERNATE(10U));

    /* SPI */
#if (HAL_USE_SPI == TRUE)
    palSetLineMode(SPI_SCK_PIN, PAL_MODE_ALTERNATE(5));
    palSetLineMode(SPI_MISO_PIN, PAL_MODE_ALTERNATE(5));
    palSetLineMode(SPI_MOSI_PIN, PAL_MODE_ALTERNATE(5));
#endif
}

bool lpm_set(pm_t mode) {
    bool ret = true;

    switch (mode) {
        case PM_SLEEP:
            /* Wake source: Any interrupt or event */
            if (power_mode != PM_RUN)
                ret = false;
            else
                SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
            break;

        case PM_STOP:
            if (power_mode != PM_RUN) ret = false;
            break;

        case PM_STANDBY:
            if (power_mode != PM_RUN)
                ret = false;
            else {
                SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
            }
            break;

        default:
            break;
    }
    power_mode = mode;

    return ret;
}

void lpm_standby(pm_t mode) {
    chSysDisable();
    wb32_set_main_clock_to_mhsi();

    rtclp_lld_init();
    stop_mode_entry();
    chSysEnable();
}

void lpm_wakeup_init(void) {
    wakeup_from_lpm = true;
    __early_init();
    wakeup_from_lpm = false;
}

void stop_mode_entry(void) {
    EXTI->PR = 0x7FFFF;
    /* NOTE: `0x01UL < j` is a comparison, not a shift, so this only ever clears
       bit 0 of each ICPR word - i.e. IRQ 0, 32, 64 ... - and leaves every other
       pending interrupt untouched. That is stock behaviour. Correcting it to
       `<<` (so that all pending interrupts really are cleared) is what stopped
       the board waking from STOP, so the original expression is kept here
       deliberately until the resume path is understood. Do not "fix" it without
       testing wake-up on hardware. */
    for (uint8_t i = 0; i < 8; i++) {
        for (uint8_t j = 0; j < 32; j++) {
            if (NVIC->ISPR[i] & (0x01UL < j)) {
                NVIC->ICPR[i] = (0x01UL < j);
            }
        }
    }
    SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk; // Clear Systick IRQ Pending

    /* Clear all bits except DBP and FCLKSD bit */
    PWR->CR0 &= 0x09U;

    /* STOP LP4 MODE S32KON */
    PWR->CR0 |= 0x3B004U;
    PWR->CFGR = 0x3B3;

    PRE_LP();

    /* Set SLEEPDEEP bit of Cortex System Control Register */
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    /* Request Wait For Interrupt */
    __WFI();

    POST_LP();

    /* Clear SLEEPDEEP bit of Cortex System Control Register */
    SCB->SCR &= (~SCB_SCR_SLEEPDEEP_Msk);

    lpm_early_wakeup();
}
