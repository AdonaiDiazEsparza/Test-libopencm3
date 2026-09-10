#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

#include "cdcacm.h"

volatile uint32_t system_millis;

void sys_tick_handler(void)
{
	system_millis++;
}

static void systick_setup(void)
{
	/* 96 MHz AHB -> 1 kHz tick */
	systick_set_frequency(1000, rcc_ahb_frequency);
	systick_clear();
	systick_interrupt_enable();
	systick_counter_enable();
}

static void clock_setup(void)
{
	/*
	 * USB FS needs a 48 MHz clock from PLLQ.
	 * HSI 16 MHz -> PLL -> SYSCLK 96 MHz, PLLQ 48 MHz.
	 * Do not use the Discovery 168 MHz / 8 MHz HSE preset on this board.
	 */
	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_3V3_96MHZ]);
}

static void gpio_setup(void)
{
	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO13);
	gpio_set(GPIOC, GPIO13);
}

static void usb_hw_setup(void)
{
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_OTGFS);

	/* PA11 = USB_DM, PA12 = USB_DP (OTG FS, AF10) */
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO11 | GPIO12);
	gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ,
				GPIO11 | GPIO12);
	gpio_set_af(GPIOA, GPIO_AF10, GPIO11 | GPIO12);
}

int main(void)
{
	uint32_t last_hello = 0;

	clock_setup();
	systick_setup();
	gpio_setup();
	usb_hw_setup();
	cdcacm_init();

	while (1) {
		cdcacm_poll();

		if ((system_millis - last_hello) >= 1000) {
			last_hello = system_millis;
			cdcacm_puts("Hola Mundo");
			gpio_toggle(GPIOC, GPIO13);
		}
	}
}
