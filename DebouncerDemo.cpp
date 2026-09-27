#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/cyw43_arch.h"
#include "di_debounce.h"

#define DI_PIN 15 //The GPIO 16 is pin 20 on the Pico board. This pin is used to read the state of the button.


int main()
{
    stdio_init_all();
    stdio_init_all();
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }

    di_init(DI_PIN);
    printf("DI debouncer running on GPIO %d\n", DI_PIN);
    while (true) {
        if (di_event_ready()) {
            printf("-> %s | first-edge t=%llu us\n",
                   di_level() ? "HIGH" : "LOW",
                   (unsigned long long)di_first_edge_us());
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN,!cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN) );
        }
        tight_loop_contents();
    }
}
