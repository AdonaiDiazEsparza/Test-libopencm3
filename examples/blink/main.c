#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

/**
* This example of a blink with systick timer
*/

// Variable to store the system milliseconds
volatile uint32_t system_millis;

/* Function Handler for the systick timer */
void sys_tick_handler(void)
{
	system_millis++;
}

/* Function to make a delay in milliseconds */
static void msleep(uint32_t delay)
{
	uint32_t start = system_millis;
	while ((system_millis - start) < delay) {
	}
}


static void systick_setup(void)
{
	/*
	* Here is configured the systick timer to get 1ms interrupts
	* We set the clock source to AHB and then we set the reload value to 15999
	* to get 1ms interrupts
	* We clear the counter and then we enable the interrupt and the counter
	*/

	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
	systick_set_reload(15999); /* 16000 - 1  , this is calculated to get 1ms */ 
	systick_clear();
	systick_interrupt_enable();
	systick_counter_enable();
}


static void clock_setup(void)
{
	/*
	* We set the clock source to HSI and then we set the system clock source to HSI
	* by default is HSI16MHz and we set the system clock source to HSI
	*/

	rcc_osc_on(RCC_HSI);
	rcc_wait_for_osc_ready(RCC_HSI);
	rcc_set_sysclk_source(RCC_CFGR_SW_HSI);
}


static void gpio_setup(void)
{
	/*
	* An easy function to set the clock of the GPIOC port and the PIN
	* We set this for the onboard LED of the STM32F411CEU6
	* the blackpill board
	*/
	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO13);
}


int main(void)
{
	clock_setup();
	systick_setup();
	gpio_setup();

	while (1) {
		gpio_toggle(GPIOC, GPIO13);
		msleep(500);
	}
}
