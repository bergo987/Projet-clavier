#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"

#define Nb_col 2
#define Nb_ligne 3

// Définition des GPIOs pour une matrice 2x3
const uint row_pins[] = {12, 8, 9};
const uint col_pins[] = {10,11};

static uint compt = 0 ; 

// Mapping des touches
char keys[Nb_ligne][Nb_col] = {
    {'A', 'B'},
    {'C', 'D'},
    {'E', 'F'}
};

int keys_pressed[Nb_ligne][Nb_col] = {
    {0,0},
    {0,0},
    {0,0}
};

int keys_to_send[Nb_ligne][Nb_col] = {
    {0,0},
    {0,0},
    {0,0}
};

int keys_pressed_long_time[Nb_ligne][Nb_col] = {
    {0,0},
    {0,0},
    {0,0}
};

void pico_init(){
    for (int i = 0; i < Nb_ligne; i++) {
        // Lignes en sortie, initialisées à HAUT
        gpio_init(row_pins[i]);
        gpio_set_dir(row_pins[i], GPIO_IN);
        gpio_put(row_pins[i], 1);
    }
    for (int i = 0; i < Nb_col; i++) {
        // Colonnes en entrée avec Pull-up
        gpio_init(col_pins[i]);
        gpio_set_dir(col_pins[i], GPIO_OUT);
        gpio_pull_up(col_pins[i]);
        
    }

    gpio_init(18);
    gpio_init(19);
    gpio_set_dir(18, GPIO_OUT);
    gpio_set_dir(19, GPIO_OUT);
}

volatile int current_col = 0;
int Nb_temps_long = 500; //Temps avant d'être dans l'état : Appuie long
int Compteur_temps_long = 5; //Compteur une fois dans l'état : Appuie long


bool repeating_timer_callback(struct repeating_timer *t) {
    gpio_put(col_pins[current_col], 1);
    current_col = (current_col + 1) % 2;
    gpio_put(col_pins[current_col], 0);
    for (int row = 0; row < Nb_ligne; row++) {
        if (gpio_get(row_pins[row])&&keys_pressed[row][current_col]==0){
            keys_to_send[row][current_col] = 1 ; 
            //printf("%c\n", keys[row][current_col]);
            if(keys_pressed_long_time[row][current_col]==1){
                keys_pressed[row][current_col]=Compteur_temps_long;
            }
            else{
                keys_pressed[row][current_col]=1;
            }
        }
        else if(gpio_get(row_pins[row])&&keys_pressed[row][current_col]!=0&&keys_pressed_long_time[row][current_col]==0)
        {
            keys_pressed[row][current_col]+=1;
            if(keys_pressed[row][current_col]==Nb_temps_long){
                keys_pressed[row][current_col]=0;
                keys_pressed_long_time[row][current_col]=1;
            }
        }
        else if(gpio_get(row_pins[row])&&keys_pressed_long_time[row][current_col]==1){
            keys_pressed[row][current_col]=(keys_pressed[row][current_col]+5)%Nb_temps_long;
        }
        else {
            keys_pressed[row][current_col]=0;
            keys_pressed_long_time[row][current_col]=0;
        }
        //printf("Key_press actuelle : row : %d, col : %d\nValeur de keys_pressed : %d\nValeur de keys_pressed_long_time : %d", row, current_col, keys_pressed[row][current_col], keys_pressed_long_time[row][current_col]);
    }
    
    gpio_put(col_pins[current_col], 1);
    current_col = (current_col + 1) % 2;
    gpio_put(col_pins[current_col], 0);
    for (int row = 0; row < 3; row++) {
        if (gpio_get(row_pins[row])&&keys_pressed[row][current_col]==0){
            keys_to_send[row][current_col] = 1 ;
            //printf("%c\n", keys[row][current_col]);
            if(keys_pressed_long_time[row][current_col]==1){
                keys_pressed[row][current_col]=Compteur_temps_long;
            }
            else{
                keys_pressed[row][current_col]=1;
            }
        }
        else if(gpio_get(row_pins[row])&&keys_pressed[row][current_col]!=0&&keys_pressed_long_time[row][current_col]==0)
        {
            keys_pressed[row][current_col]+=1;
            if(keys_pressed[row][current_col]==Nb_temps_long){
                keys_pressed[row][current_col]=0;
                keys_pressed_long_time[row][current_col]=1;
            }
        }
        else if(gpio_get(row_pins[row])&&keys_pressed_long_time[row][current_col]==1){
            keys_pressed[row][current_col]=(keys_pressed[row][current_col]+5)%Nb_temps_long;
        }
        else {
            keys_pressed[row][current_col]=0;
            keys_pressed_long_time[row][current_col]=0;
        }
        //printf("Key_press actuelle : row : %d, col : %d\nValeur de keys_pressed : %d\nValeur de keys_pressed_long_time : %d", row, current_col, keys_pressed[row][current_col], keys_pressed_long_time[row][current_col]);
    }
    return true; 
}

int main() {
    stdio_init_all();
    pico_init();
    
    struct repeating_timer timer;
    add_repeating_timer_us(-1000, repeating_timer_callback, NULL, &timer);
    
    while (1) {
        if (compt){
            gpio_put(18,0);
            gpio_put(19,1);
            compt = 0; 
        }
        else {
            gpio_put(19,0);
            gpio_put(18,1);
            compt +=1 ; 
        }
        sleep_ms(500);
    }
}