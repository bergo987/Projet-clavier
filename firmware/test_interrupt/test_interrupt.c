#include <stdio.h>
#include "pico/stdlib.h"

#define LED_DELAY_MS 250

static uint compteur = 0;

// Perform initialisation
int pico_led_init(void) {
    // A device like Pico that uses a GPIO for the LED will define PICO_DEFAULT_LED_PIN
    // so we can use normal GPIO functionality to turn the led on and off
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
}

// Turn the led on or off
void pico_set_led(bool led_on) {
    // Just set the GPIO on or off
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
}

// Interruption
bool repeating_timer_callback(struct repeating_timer *t)
{   
    if (compteur){
       pico_set_led(true); 
       compteur = 0 ; 
       printf("Led : On \n"); 
    }
    else {
    compteur += 1 ; 
    pico_set_led(false);
    printf("Led : OFF \n"); 
    }    
  return true;

}// Fin interruption

int main() {
    stdio_init_all(); // On en a besoin pour setup la pi correctement 

    int rc = pico_led_init();
    hard_assert(rc == PICO_OK);
    
    struct repeating_timer timer;

    add_repeating_timer_ms(2000, repeating_timer_callback, NULL, &timer);

    while (true) {
    }
}
