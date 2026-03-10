#include <stdio.h>
#include "pico/stdlib.h"

#define LED_DELAY_MS 250

static uint compteur = 0; 
// Perform initialisation
int pico_init(void) {
    // A device like Pico that uses a GPIO for the LED will define PICO_DEFAULT_LED_PIN
    // so we can use normal GPIO functionality to turn the led on and off
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    gpio_init(12);
    gpio_init(8);
    gpio_init(9);
    gpio_init(10);
    gpio_init(11);

    gpio_init(18); 
    gpio_init(19); 
    gpio_set_dir(12, GPIO_IN);
    gpio_set_dir(8, GPIO_IN);
    gpio_set_dir(9, GPIO_IN);
    gpio_set_dir(10, GPIO_OUT);
    gpio_set_dir(11, GPIO_OUT);
    gpio_set_dir(18, GPIO_OUT);
    gpio_set_dir(19, GPIO_OUT);

    return PICO_OK;
}

// Turn the led on or off
void pico_set_led(bool led_on) {
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
}

bool repeating_timer_callback(struct repeating_timer *t)
{   
    gpio_put(10,1);
    gpio_put(11,0);
    if(gpio_get(12)){
        printf("A\n");
        //pico_set_led(true);
    }
    else{
        printf("Non\n");
    }
    if(gpio_get(8)){
        printf("C\n");
        //pico_set_led(true);
    }
    else{
        printf("Non\n");
    }
    if(gpio_get(9)){
        printf("E\n");
        //pico_set_led(true);
    }
    else{
        printf("Non\n");
    }
    //pico_set_led(false);
    gpio_put(10,0);
    sleep_ms(10);
    gpio_put(11,1);
    sleep_ms(10);
    if(gpio_get(12)){
        printf("B\n");
        //pico_set_led(true);
    }
    else{
        printf("Non\n");
    }
    if(gpio_get(8)){
        printf("D\n");
        //pico_set_led(true);
    }
    else{
        printf("Non\n");
    }
    if(gpio_get(9)){
        printf("F\n");
        //pico_set_led(true);
    }
    else{
        printf("Non\n");
    }
    return true; 
}// Fin interruption




int main()
{
    stdio_init_all();
    pico_init();
    //gpio_set_irq_enabled(12, true, true);
    //gpio_set_irq_callback(led_allume);
    //if (true) irq_set_enabled(IO_IRQ_BANK0, true);

    struct repeating_timer timer;

    add_repeating_timer_ms(10, repeating_timer_callback, NULL, &timer);

    while (1){
      if (compteur){
        gpio_put(18,1);
        gpio_put(19,0);
        compteur = 0 ; 
        printf("Led : Rouge \n"); 
    }
    else {
        compteur += 1 ; 
        gpio_put(18,0);
        gpio_put(19,1);
        printf("Led : Orange \n"); 
        }      
    sleep_ms(500);
    }    

    return 0 ; 
}
