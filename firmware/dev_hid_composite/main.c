#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SSD1306_IMPL

#include "ssd1306.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"
#include "hardware/adc.h"


#include "bsp/board_api.h"
#include "tusb.h"

#include "usb_descriptors.h"

//OLED PINS : SDA : GPIO8 SCL : GPIO9
//GPIO27

#define I2C_PORT i2c0
#define SDA_PIN 8
#define SCL_PIN 9

#define Nb_col 4
#define Nb_ligne 5

#define WS2812_PIN 22
#define NUM_PIXELS 20

#define IS_RGBW false
//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
const uint32_t blink_led = 500; 
const uint32_t blink_RGB = 30; 

void update_oled_display(int val);
void led_blinking_task(void);
void hid_task(void);
void led_blinking_task_2(PIO pio, uint sm, uint len, uint t); 
void pattern_wave(PIO pio, uint sm, uint len, uint t, uint s);
void pattern_rainbow(PIO pio, uint sm, uint len, uint t, uint s);
void pattern_reaction(PIO pio, uint sm, uint len, uint t, uint s);
uint32_t hsv_to_urgb(uint8_t h, uint8_t s, uint8_t v);
int RGB(PIO pio, uint sm, uint len, uint t);
void pico_init(); 

//-------------------------------------------------------------------+
// KEYBOARD MATRIX 
//-------------------------------------------------------------------+

// GPIO17 pour le potar
const uint row_pins[] = {15,
                        14,
                        13,
                        12,
                        11};
const uint col_pins[] = {18,19,20,21};
// Mapping des touche
const const uint8_t hid_keys[Nb_ligne][Nb_col] = {
    {HID_KEY_A, HID_KEY_B,HID_KEY_G,HID_KEY_H},
    {HID_KEY_C, HID_KEY_D, HID_KEY_I, HID_KEY_J},
    {HID_KEY_Q, HID_KEY_Z,HID_KEY_K,HID_KEY_L},
    {HID_KEY_SHIFT_LEFT, HID_KEY_N, HID_KEY_O, HID_KEY_P},
    {HID_KEY_CONTROL_LEFT, HID_KEY_R, HID_KEY_S, HID_KEY_T}
};

volatile int keys_pressed[Nb_ligne][Nb_col] = {
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
};

int keys_to_send[Nb_ligne][Nb_col] = {
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
};

int keys_pressed_long_time[Nb_ligne][Nb_col] = {
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
};

volatile int led_life[Nb_ligne][Nb_col] = {
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
};

volatile int led_life_next[Nb_ligne][Nb_col] = {
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
};

volatile int potar = 0;
volatile int keys_pressed_potar = 0;
volatile int keys_pressed_potar_long_time = 0;

int Nb_temps_long = 500; //Temps avant d'être dans l'état : Appuie long 
int Compteur_temps_long = 5; //Compteur une fois dans l'état : Appuie long

bool repeating_timer_callback(struct repeating_timer *t) {
    if ((gpio_get(17)==0)&&(keys_pressed_potar == 0)){
      potar = (potar + 1)%3;
      if(keys_pressed_potar_long_time == 1){
        keys_pressed_potar = Compteur_temps_long;
      } else {
        keys_pressed_potar = 1;
      }
    } else if ( gpio_get(17)==0 && keys_pressed_potar!=0 && keys_pressed_potar_long_time==0 ){
      keys_pressed_potar+=1;
      if(keys_pressed_potar>=Nb_temps_long){
        keys_pressed_potar=0;
        keys_pressed_potar_long_time=1;
      }
    } else if (gpio_get(17)==0 &&keys_pressed_potar_long_time==1) {
      keys_pressed_potar=(keys_pressed_potar+Compteur_temps_long)%Nb_temps_long;
    } else {
        // BOUTON RELÂCHÉ : On réinitialise tout pour le prochain appui
        keys_pressed_potar = 0;
        keys_pressed_potar_long_time = 0;
    }
    for(int init = 0; init < Nb_col; init++){
      gpio_put(col_pins[init], 0);
    }
    for(int col = 0; col < Nb_col; col++){
      gpio_put(col_pins[col], 1);

      busy_wait_us(2);

      for (int row = 0; row < Nb_ligne; row++) {
          if (gpio_get(row_pins[row])&&(keys_pressed[row][col]==0)){
              keys_to_send[row][col] = 1 ; 
              led_life[row][col] = 10 ;
              if(keys_pressed_long_time[row][col]==1){
                  keys_pressed[row][col]=Compteur_temps_long;
              } else {
                  keys_pressed[row][col]=1;
              }
          } 
          else if(gpio_get(row_pins[row])&&(keys_pressed[row][col]!=0)&&(keys_pressed_long_time[row][col]==0))
          {
              keys_pressed[row][col]+=1;
              if((hid_keys[row][col]==HID_KEY_CONTROL_RIGHT || hid_keys[row][col]==HID_KEY_SHIFT_LEFT) && keys_pressed[row][col]>=Nb_temps_long/10){
                keys_pressed[row][col]=0;
                keys_pressed_long_time[row][col]=1;
              }
              else if(keys_pressed[row][col]>=Nb_temps_long){
                keys_pressed[row][col]=0;
                // keys_to_send[row][col] = 0 ; 
                keys_pressed_long_time[row][col]= 1 ;
              }
          }
          else if(gpio_get(row_pins[row])&&keys_pressed_long_time[row][col]==1){
            if(hid_keys[row][col]==HID_KEY_CONTROL_RIGHT || hid_keys[row][col]==HID_KEY_SHIFT_LEFT){
              keys_pressed[row][col] = 1 ;
              keys_to_send[row][col] = 1 ;
            } else {
              keys_pressed[row][col]=(keys_pressed[row][col]+Compteur_temps_long)%Nb_temps_long;
            }
          } else {
            if(hid_keys[row][col]==HID_KEY_CONTROL_RIGHT || hid_keys[row][col]==HID_KEY_SHIFT_LEFT){
              keys_pressed[row][col]=0;
              keys_pressed_long_time[row][col]=0;
            } else {
              keys_to_send[row][col] = 0 ; 
              keys_pressed[row][col]= 0 ;
              keys_pressed_long_time[row][col]= 0 ;
            }
          }
        } 
      gpio_put(col_pins[col], 0);
    }
    return true; 
}



static inline void put_pixel(PIO pio, uint sm, uint32_t pixel_grb) {
    pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}

/*------------- MAIN -------------*/

int main(void)
{
  board_init();
  stdio_init_all();
  pico_init(); 
  update_oled_display(0);
  struct repeating_timer timer;
  add_repeating_timer_us(-1000, repeating_timer_callback, NULL, &timer);

  int last_known_potar = -1; // Pour forcer la première mise à jour
  int compteur_actu = 0;
  // initialisation de Tiny usb
  tud_init(BOARD_TUD_RHPORT);
  if (board_init_after_tusb) {
    board_init_after_tusb();
  }
  //Initialisation des LED
  PIO pio;
  uint sm;
  uint offset;
  // This will find a free pio and state machine for our program and load it for us
  // We use pio_claim_free_sm_and_add_program_for_gpio_range (for_gpio_range variant)
  // so we will get a PIO instance suitable for addressing gpios >= 32 if needed and supported by the hardware
  bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &pio, &sm, &offset, WS2812_PIN, 1, true);
  hard_assert(success);

  ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);
  static uint compt = 0 ; 
  uint t = 0;  
  while (1)
  {
    tud_task(); // tinyusb device task
    led_blinking_task();
    compteur_actu++;
    if(compteur_actu>1000){
      update_oled_display(compteur_actu);
      compteur_actu=0;
    }
    int incr = RGB(pio,sm,NUM_PIXELS,t); 
    if (potar != last_known_potar) {
            update_oled_display(compteur_actu); // on mets à jour directement l'oled sans attendre
            last_known_potar = potar; // On mémorise le nouvel état
        }
    if(incr == 1 ){
      t = (t+1);
      incr = 0 ;
    }

    hid_task();
  }
pio_remove_program_and_unclaim_sm(&ws2812_program, pio, sm, offset);

}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+
static void send_hid_report(uint8_t report_id)
{
  // skip if hid is not ready yet
  if ( !tud_hid_ready() ) return;

  switch(report_id)
  {
    case REPORT_ID_KEYBOARD:
    {
      // use to avoid send multiple consecutive zero report for keyboard
      static bool has_keyboard_key = false;
      uint8_t keycode[6] = { 0 };
      uint8_t indice = 0;
      for (int col = 0; col < Nb_col; col++){
        for (int row = 0; row < Nb_ligne; row++) {
              if(keys_to_send[row][col]==1 && indice!= 6){ 
                keycode[indice] = hid_keys[row][col];
                keys_to_send[row][col]=0;
                indice++;
              }
            }
          }   
          tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
      has_keyboard_key = false;
    }
    break;
    default: break;
  }

}

void hid_task(void)
{

  const uint32_t interval_ms = 10; // durée entre deux déclenchement 
  static uint32_t start_ms = 0; // debut du compteur 

  if ( board_millis() - start_ms < interval_ms)// on sort de la fonction
    {return;} 
  start_ms += interval_ms; //mise à jour du compteur
  
  send_hid_report(REPORT_ID_KEYBOARD); // execution de la tache 
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;
  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  (void) instance;
  if (report_type == HID_REPORT_TYPE_OUTPUT)
  {
    // Set keyboard LED e.g Capslock, Numlock etc...
    if (report_id == REPORT_ID_KEYBOARD)
    {
      // bufsize should be (at least) 1
      if ( bufsize < 1 ) return;
      uint8_t const kbd_leds = buffer[0];
      if (kbd_leds & KEYBOARD_LED_CAPSLOCK)
      {
        // Capslock On: disable blink, turn led on
        blink_interval_ms = 0;
        board_led_write(true);
      }else
      {
        // Caplocks Off: back to normal blink
        board_led_write(false);
        blink_interval_ms = BLINK_MOUNTED;
      }
    }
  }
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // blink is disabled
  if (!blink_interval_ms) return;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}

int RGB(PIO pio, uint sm, uint len, uint t)
{
  int res = 0 ; 
  static uint32_t start_ms = 0;
  static bool led_state = false; 
  // blink is disabled
  if (!blink_led) return res ;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_RGB) return res ; // not enough time
  start_ms += blink_RGB;

  if (led_state){
    if (potar == 0) {
      pattern_rainbow(pio,sm,len,t, 0);
    }
    else if(potar == 1) {
      pattern_wave(pio,sm,len,t, 0);
    } else {
      pattern_reaction(pio,sm,len,t,0);
    }
    res = 1 ; 
  }
  else {
    //for (int i = 0; i < NUM_PIXELS ; i++)
      //put_pixel(pio,sm,urgb_u32(0, 0, 0));
      res = 0 ; 
  }
  led_state = 1 - led_state; // toggle
  return res ; 
}

void pattern_reaction(PIO pio, uint sm, uint len, uint t, uint s) {
    // On prépare le tableau pour la prochaine étape
    uint8_t next_life[Nb_ligne][Nb_col];
    
    // On commence par copier l'état actuel pour le modifier
    for (int j = 0; j < Nb_ligne; j++) {
        for (int i = 0; i < Nb_col; i++) {
            next_life[j][i] = led_life[j][i];
        }
    }

    for (uint j = 0; j < Nb_ligne; j++) {
        for (uint i = 0; i < Nb_col; i++) {
            
            if (led_life[j][i] > 0) {
                // 1. AFFICHAGE
                // On augmente la luminosité (vie * 20) pour que ce soit bien visible
                // 10 * 20 = 200 (sur un max de 255)
                put_pixel(pio, sm, urgb_u32(led_life[j][i] * 1, 0, 0));

                // 2. PROPAGATION (seulement si la LED vient d'être activée)
                if (led_life[j][i] == 10) {
                    if (j > 0 && led_life[j-1][i] == 0) next_life[j-1][i] = 10;
                    if (j < Nb_ligne - 1 && led_life[j+1][i] == 0) next_life[j+1][i] = 10;
                    if (i > 0 && led_life[j][i-1] == 0) next_life[j][i-1] = 10;
                    if (i < Nb_col - 1 && led_life[j][i+1] == 0) next_life[j][i+1] = 10;
                }

                // 3. EXTINCTION PROGRESSIVE
                // Si la LED n'est pas en train d'être activée par un voisin (donc pas 10)
                // on diminue sa vie
                if (next_life[j][i] > 0) {
                    next_life[j][i]--;
                }
            } 
            else {
                put_pixel(pio, sm, 0); // Éteint
            }
        }
    }

    // Mise à jour de la matrice globale
    for (uint j = 0; j < Nb_ligne; j++) {
        for (uint i = 0; i < Nb_col; i++) {
            led_life[j][i] = next_life[j][i];
        }
    }
}

void pattern_wave(PIO pio, uint sm, uint len, uint t, uint s){
    int intensite_max = 50;
    int longueur_wave = len;
    int milieu_wave = longueur_wave / 2;
    int a ; 
  for(uint i = 0; i< len; i++) {  
    if(s){
      a = i + t ;
    } else {
      a = (len - i ) - t ;
    }
    int x = a % (int)len;
    if (x < 0) x += len;
    if (x < longueur_wave){
      int distance_centre = abs(x - milieu_wave); // Calcul ditance au centre des LEDs
      int r_g;
      if (distance_centre == 0) {
          r_g = intensite_max;
      } else {
          // r_g = intensite_max / (distance_centre * distance_centre + 1); // Pour forte intensité max
          r_g = intensite_max / (2*distance_centre); // Pour faible intensité max
      }
      put_pixel(pio, sm, urgb_u32((uint8_t)r_g, r_g, 0));
        //put_pixel(pio, sm, urgb_u32(50, 0, 0));
    } else { 
      put_pixel(pio, sm, urgb_u32(0, 0, 0));
    }
  }
}

// Fonction utilitaire pour convertir HSV en RGB (format 32 bits pour PIO)
uint32_t hsv_to_urgb(uint8_t h, uint8_t s, uint8_t v) {
    uint8_t r, g, b;
    uint8_t region, remainder, p, q, t;

    if (s == 0) {
        return urgb_u32(v, v, v);
    }

    region = h / 43;
    remainder = (h - (region * 43)) * 6;

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return urgb_u32(r, g, b);
}

void pattern_rainbow(PIO pio, uint sm, uint len, uint t, uint s) {
    // Choix du nombre de couleur
    uint8_t densite = 5; 
    // Choix de la vitesse 
    uint16_t temps_ralenti = 5*t; 

    for (uint i = 0; i < len; i++) {
        uint8_t hue;
        
        if (s) {
            // i * densite : position de la couleur sur le clavier
            // + temps_ralenti : défilement de l'animation
            hue = (uint8_t)((i * densite) + temps_ralenti);
        } else {
            hue = (uint8_t)(((len - i) * densite) + temps_ralenti);
        }

        put_pixel(pio, sm, hsv_to_urgb(hue, 255, 10));
    }
}

void update_oled_display(int val) {
    // 1. Lecture de l'ADC

    adc_select_input(1); // Sélectionne l'entrée 1 (GPIO 28)
    uint16_t raw1 = adc_read();

    busy_wait_us(1);
    // 2. Conversion en Tension (V)
    //float current = (raw1 +val) /(2.0f * 4095.0f);


    adc_select_input(2); // Sélectionne l'entrée 1 (GPIO 28)
    uint16_t raw2 = adc_read();
    // char amp_str0[20];
    // sprintf(amp_str0,"%d", raw2);
    float current = (raw2 - raw1)/(4095.0f * 0.1f); 
    ssd1306_clear();
    
    // Affichage du Titre
    // ssd1306_draw_string(0, 0, amp_str0);
     ssd1306_draw_string(0, 0, "KEYBOARD");
    
    // Affichage du Mode actuel (Potar)
    char mode_str[20];
    switch(potar) {
        case 0: sprintf(mode_str, "RAINBOW"); break;
        case 1: sprintf(mode_str, "WAVE"); break;
        case 2: sprintf(mode_str, "REACTION"); break;
    }
    ssd1306_draw_string(0, 2, mode_str);
    
    // Affichage de la valeur de courant
    char amp_str[20];
    sprintf(amp_str, "CUR: %.2fA", current); // %.2f pour 2 décimales
    // char amp_str1[20];
    // sprintf(amp_str1,"%d", raw1);
    //ssd1306_draw_string(0, 4, amp_str1);
    ssd1306_draw_string(0, 4, amp_str);
    
    ssd1306_flush();
}

//---------------------------------------
// Init
//---------------------------------------

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

    adc_init();
    adc_gpio_init(27); // Prépare le GPIO 27 pour l'ADC

    adc_gpio_init(28); // Prépare le GPIO 28 pour l'ADC
    // Initialisation de l'écran via ta bibliothèque
    ssd1306_init();
    gpio_init(17);
    gpio_init(18);
    gpio_init(19);
    gpio_set_dir(17, GPIO_IN);
    gpio_set_dir(18, GPIO_OUT);
    gpio_set_dir(19, GPIO_OUT);
}