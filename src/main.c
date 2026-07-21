#include <bsp/board_api.h>
#include <pico/stdio.h>
#include <tusb.h>

#include "usb_protocol.h"

#include <pico/stdlib.h>

#ifndef PICO_DEFAULT_LED_PIN
#warning blink_simple example requires a board with a regular LED
#endif

// Initialize the GPIO for the LED
void pico_led_init(void)
{
#ifdef PICO_DEFAULT_LED_PIN
    // A device like Pico that uses a GPIO for the LED will define PICO_DEFAULT_LED_PIN
    // so we can use normal GPIO functionality to turn the led on and off
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif
}

// Turn the LED on or off
void pico_set_led(bool led_on)
{
#if defined(PICO_DEFAULT_LED_PIN)
    // Just set the GPIO on or off
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#endif
}

int main(void)
{
    board_init();
    tusb_init();

    if (board_init_after_tusb)
    {
        board_init_after_tusb();
    }

    stdio_init_all();
    usb_protocol_init();

    pico_led_init();
    pico_set_led(true);

    while (true)
    {
        tud_task();
        usb_protocol_task();
    }

    return 0;
}
