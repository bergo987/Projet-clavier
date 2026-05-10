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

#include "hardware/flash.h"
#include "hardware/sync.h"

// Initilisation du port des PINs pour l'OLED
#define I2C_PORT i2c0
#define SDA_PIN 8
#define SCL_PIN 9

// Choix de la taille de la matrice de touche
#define Nb_col 4
#define Nb_ligne 5
#define NUM_PIXELS 20

// Choix du PIN pour l'envoi du bus de LEDs
#define WS2812_PIN 22
#define IS_RGBW false

// Définition du nombre de mode pour motif de LEDs
#define NB_MODE 4 

// Nous choisissons une zone à la fin de la flash (ex: 1.5Mo après le début)
#define FLASH_TARGET_OFFSET (1536 * 1024)
const uint8_t *flash_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);


#define FLASH_MAGIC 0xAB
#define FLASH_VERSION __TIME__  // Change à chaque compilation

void __not_in_flash_func(save_mapping_to_flash)(uint8_t keys[Nb_ligne][Nb_col], uint8_t hues[20]) {
    uint32_t ints = save_and_disable_interrupts();
    
    // Nous effacons le secteur (nécessaire avant l'écrire)
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    
    uint8_t buffer[FLASH_PAGE_SIZE] = {0};
    buffer[0] = FLASH_MAGIC;  // Octet de contrôle

    // Nous copions le temps __TIME__ 
    memcpy(buffer + 1, __TIME__, 8);  // 8 caractères ex: "14:32:05"

    // Nous copions les 20 octets de touches à partir de l'index 9 - Le mapping de touche 
    memcpy(buffer + 9, keys, 20); 
    
    // Nous copions les 20 octets de couleurs à partir de l'index 29 - La couleur des LEDs
    memcpy(buffer + 29, hues, 20);
    
    // Nous écrivons le tout d'un coup
    flash_range_program(FLASH_TARGET_OFFSET, buffer, FLASH_PAGE_SIZE);
    
    restore_interrupts(ints);
}

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

// Variable globale pour stocker le timestamp de l'appui
volatile uint32_t last_key_press_time = 0;
volatile uint32_t last_latency_ms = 0; 

// --------------- Appel des fonctions ----------------

void pico_init(); 

void hid_task(void);

void led_blinking_task(void);

uint32_t hsv_to_urgb(uint8_t h, uint8_t s, uint8_t v);
int RGB(PIO pio, uint sm, uint len, uint t);

void pattern_wave(PIO pio, uint sm, uint len, uint t, uint s);
void pattern_rainbow(PIO pio, uint sm, uint len, uint t, uint s);
void pattern_reaction(PIO pio, uint sm, uint len, uint t, uint s);
void pattern_custom_individual(PIO pio, uint sm);

void update_oled_display();

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts);
void tud_cdc_rx_cb(uint8_t itf);

//-------------------------------------------------------------------+
// KEYBOARD MATRIX MAPPING
//-------------------------------------------------------------------+

// Définition des Ports de GPIO pour la mapping des touches
const uint row_pins[] = {15,
                        14,
                        13,
                        12,
                        11};
const uint col_pins[] = {18,19,20,21};

// Mapping des touches par défaut 
uint8_t hid_keys[Nb_ligne][Nb_col] = {
    {HID_KEY_A,             HID_KEY_B,  HID_KEY_G,  HID_KEY_H},
    {HID_KEY_C,             HID_KEY_D,  HID_KEY_I,  HID_KEY_J},
    {HID_KEY_Q,             HID_KEY_Z,  HID_KEY_K,  HID_KEY_L},
    {HID_KEY_SHIFT_LEFT,    HID_KEY_N,  HID_KEY_O,  HID_KEY_P},
    {HID_KEY_CONTROL_LEFT,  HID_KEY_R,  HID_KEY_S,  HID_KEY_T}
};

// Matrice pour la détection de touche & temps d'appui
volatile int keys_pressed[Nb_ligne][Nb_col] = {
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
};

// Matrice pour savoir quelle touche envoyer à l'HID
int keys_to_send[Nb_ligne][Nb_col] = {
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
};

// Matrice pour le statut d'appui long d'une touche ou non ( Si 1 -> Appui long )
int keys_pressed_long_time[Nb_ligne][Nb_col] = {
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
};

//-------------------------------------------------------------------+
// KEYBOARD MATRIX LEDs
//-------------------------------------------------------------------+

//----------------- Matrices  pour le mode "REACTION" des LEDS ----------------------
// Permet de connaître l'emplacement de la LED actuelle et de sa durée de vie
volatile int led_life[Nb_ligne][Nb_col] = {
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
    {0,0,0,0},
};

// Permet de connaître l'emplacement de la prochaine LED et de sa durée de vie restante
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

uint8_t led_hues[20] = {0}; // Stocke la teinte de chaque LED (0-255)

bool repeating_timer_callback(struct repeating_timer *t) {
  //------------ Lecture de l'appui du Potar --------------
  if ((gpio_get(17)==0)&&(keys_pressed_potar == 0)){ // Détection d'un appui nouveau
    potar = (potar + 1)%NB_MODE;
    if(keys_pressed_potar_long_time == 1){
      keys_pressed_potar = Compteur_temps_long;
    } else {
      keys_pressed_potar = 1;
    }
  } else if ( gpio_get(17)==0 && keys_pressed_potar!=0 && keys_pressed_potar_long_time==0 ){ // Détection d'un appui en temps court
    keys_pressed_potar+=1;
    if(keys_pressed_potar>=Nb_temps_long){
      keys_pressed_potar=0;
      keys_pressed_potar_long_time=1;
    }
  } else if (gpio_get(17)==0 &&keys_pressed_potar_long_time==1) { // Détection d'un appui en temps long
    keys_pressed_potar=(keys_pressed_potar+Compteur_temps_long)%Nb_temps_long;
  } else {  // Pas d'appui
      keys_pressed_potar = 0;
      keys_pressed_potar_long_time = 0;
  }

  //------------ Ensemble de colonnes mis à 0 avant lecture de la matrice --------------
  for(int init = 0; init < Nb_col; init++){
    gpio_put(col_pins[init], 0);
  }

  //------------ Détection de l'appui de touche sur l'ensemble de la matrice --------------
  for(int col = 0; col < Nb_col; col++){
    gpio_put(col_pins[col], 1); // Colonne à un 1 logique pour détecter l'appui d'une touche
    busy_wait_us(2); // Temps de pause pour la stabilisation électriques des GPIOs de colonnes

    for (int row = 0; row < Nb_ligne; row++) {
      if (gpio_get(row_pins[row])&&(keys_pressed[row][col]==0)) { // Détection d'un appui nouveau
        keys_to_send[row][col] = 1 ; 
        led_life[row][col] = 10 ;
        last_key_press_time = board_millis();  // début du timestamp 
        if(keys_pressed_long_time[row][col]==1){
          keys_pressed[row][col]=Compteur_temps_long;
        } else {
          keys_pressed[row][col]=1;
        }
      } else if(gpio_get(row_pins[row])&&(keys_pressed[row][col]!=0)&&(keys_pressed_long_time[row][col]==0)) { // Détection d'un appui en temps court
        keys_pressed[row][col]+=1;
        if((hid_keys[row][col]==HID_KEY_CONTROL_RIGHT || hid_keys[row][col]==HID_KEY_SHIFT_LEFT) && keys_pressed[row][col]>=Nb_temps_long/10){
          keys_pressed[row][col]=0;
          keys_pressed_long_time[row][col]=1;
        } else if(keys_pressed[row][col]>=Nb_temps_long){
          keys_pressed[row][col]=0;
          keys_pressed_long_time[row][col]= 1 ;
        }
      } else if(gpio_get(row_pins[row])&&keys_pressed_long_time[row][col]==1) { // Détection d'un appui en temps long
        if(hid_keys[row][col]==HID_KEY_CONTROL_RIGHT || hid_keys[row][col]==HID_KEY_SHIFT_LEFT){
          keys_pressed[row][col] = 1 ;
          keys_to_send[row][col] = 1 ;
        } else {
          keys_pressed[row][col]=(keys_pressed[row][col]+Compteur_temps_long)%Nb_temps_long;
        }
      } else { // Pas d'appui
        if (keys_pressed[row][col] != 0) {
          // La touche vient d'être relâchée → on calcule la latence
          last_latency_ms = board_millis() - last_key_press_time;
        }
        keys_to_send[row][col] = 0 ; 
        keys_pressed[row][col]= 0 ;
        keys_pressed_long_time[row][col]= 0 ;
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
    return ( ((uint32_t) (r) << 8) | ((uint32_t) (g) << 16) | (uint32_t) (b) );
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
    (void) itf;
    (void) rts;

    // Si DTR est true, le PC vient d'ouvrir le port COM
    if ( dtr ) {
        // Optionnel : Peut servir à montrer visuellement si l'ouverture du port est fonctionnel
    }
}

// Cette fonction est appelée dès que des données arrivent
void tud_cdc_rx_cb(uint8_t itf) {
    (void) itf;
}

/*------------- MAIN -------------*/

int main(void)
{
   // --------------- Initilisation des ports GPIOs, OLED et autres -----------------
  board_init();
  stdio_init_all();
  pico_init(); 

   // --------------- Lecture de la ram au démarrage / Lecture par défaut au nouveau BOOT --------------
  if (flash_contents[0] == FLASH_MAGIC && memcmp(flash_contents + 1, __TIME__, 8) == 0) {
    memcpy(hid_keys, flash_contents + 9, 20); // Copie du mapping de touches
    memcpy(led_hues, flash_contents + 29, 20); // Copie du mapping de LEDs
  } 
  // --------------- Lecture de l'écran OLED ---------------
  update_oled_display();

  // --------------- Initilisation de la fonction callback ( Lecture de la matrice périodiquement avec T = 1ms ) ------------
  struct repeating_timer timer;
  add_repeating_timer_us(-1000, repeating_timer_callback, NULL, &timer);

   // --------------- Variable nécessaire à l'actualisation de l'écran OLED ------------------
  int last_known_potar = -1;
  int compteur_actu = 0;

  // ---------------- Initialisation de Tiny usb -----------------
  tud_init(BOARD_TUD_RHPORT);
  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  PIO pio;
  uint sm;
  uint offset;

  // --------------- Vérification de la disponibilité de PIO / SM et Offset -----------------
  bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &pio, &sm, &offset, WS2812_PIN, 1, true);
  hard_assert(success);

  // --------------- Initialisation des LEDs sur GPIO, SM ( State Machine ) à 800khz ---------------
  ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

  uint t = 0;

  // --------------- Gestion du CDC ---------------
  static uint8_t cdc_buffer[20];
  static uint8_t cdc_idx = 0;
  static bool cdc_connected = false;

  while (1)
  {
    tud_task();
    led_blinking_task();

    // --------------- Actualisation de l'écran OLED tous les 100000 cycles d'incrémentation de compteur_actu -----------
    compteur_actu++;
    if (compteur_actu > 100000) {
      update_oled_display();
      compteur_actu = 0;
    }

    int incr = RGB(pio, sm, NUM_PIXELS, t);

     // --------------- Actualisation de l'écran OLED à chaque changement de mode de LEDs -------------
    if (potar != last_known_potar) {
      update_oled_display();
      last_known_potar = potar;
    }

    if (incr == 1) {
      t = (t + 1);
      incr = 0;
    }

    hid_task();

    // Reset du buffer si déconnexion/reconnexion
    bool currently_connected = tud_cdc_connected();
    if (!currently_connected && cdc_connected) {
        cdc_idx = 0;
    }
    cdc_connected = currently_connected;

    // --------------- Gestion du CDC (40 octets : 20 touches + 20 couleurs) ---------------
    static uint8_t cdc_buffer[40]; 

    
    while (tud_cdc_available()) {
        uint8_t byte = tud_cdc_read_char();

        // Permet d'ignorer les index hors limites
        if (cdc_idx >= 40) {
            cdc_idx = 0;
        }

        cdc_buffer[cdc_idx++] = byte;

        if (cdc_idx == 40) {
            // Nous récupérons les touches (20 premiers octets)
            memcpy(hid_keys, cdc_buffer, 20);
            
            // Nous récupérons les couleurs des LEDs (20 octets suivants)
            memcpy(led_hues, cdc_buffer + 20, 20);

            // Sauvegarde des touches et couleurs des LEDs dans la flash
            save_mapping_to_flash(hid_keys, led_hues);

            // Feedback série de debug - à lire dans le terminal 
            char debug[64];
            sprintf(debug, "OK! Keys received.\nLED[0] Hue: %02X\n\nKEYS[0,0] : %02X", led_hues[0], hid_keys[0][0]);
            tud_cdc_write(debug, strlen(debug));
            tud_cdc_write_flush();

            cdc_idx = 0; // Prêt pour le prochain envoi
        }
    }
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
      static bool has_keyboard_key = false;
      uint8_t keycode[6] = { 0 };
      uint8_t indice = 0;
      for (int col = 0; col < Nb_col; col++){
          for (int row = 0; row < Nb_ligne; row++) {
              if(keys_to_send[row][col]==1 && indice != 6){ 
                  // Debug : affiche ce que nous allons envoyer
                  char debug[32];
                  sprintf(debug, "sending [%d][%d]=%02X\n", row, col, hid_keys[row][col]);
                  tud_cdc_write(debug, strlen(debug));
                  tud_cdc_write_flush();
                  
                  keycode[indice] = hid_keys[row][col];
                  keys_to_send[row][col] = 0;
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

  if ( board_millis() - start_ms < interval_ms)// nous sortons de la fonction
    {return;} 
  start_ms += interval_ms; //mise à jour du compteur
  
  send_hid_report(REPORT_ID_KEYBOARD);
  
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

// -------------- Choix du pattern des LEDs et des timings d'affichage ---------------
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
    } else if(potar == 1) {
      pattern_wave(pio,sm,len,t, 0);
    } else if(potar ==2) {
      pattern_reaction(pio,sm,len,t,0);
    } else { 
      pattern_custom_individual(pio,sm);
    }
    res = 1 ; 
  }
  else {
      res = 0 ; 
  }
  led_state = 1 - led_state; // toggle
  return res ; 
}

// --------------- Pattern réaction de touche et propagation ----------------
void pattern_reaction(PIO pio, uint sm, uint len, uint t, uint s) {
    // Initilisation du prochain tableau
    uint8_t next_life[Nb_ligne][Nb_col];
    
    // Copie de l'état actuel du tableau pour le modifier
    for (int j = 0; j < Nb_ligne; j++) {
        for (int i = 0; i < Nb_col; i++) {
            next_life[j][i] = led_life[j][i];
        }
    }
    // Parcour de l'ensemble du tableau
    for (uint j = 0; j < Nb_ligne; j++) {
        for (uint i = 0; i < Nb_col; i++) {
            if (led_life[j][i] > 0) {
                // 1. Nous affichons les leds du tableau actuel
                put_pixel(pio, sm, urgb_u32(led_life[j][i], 0, 0));

                // 2. Propagation des LEDs (seulement si la LED vient d'être activée)
                if (led_life[j][i] == 10) {
                    if (j > 0 && led_life[j-1][i] == 0)             next_life[j-1][i] = 10; // Propagation vers la gauche
                    if (j < Nb_ligne - 1 && led_life[j+1][i] == 0)  next_life[j+1][i] = 10; // Propagation vers la droite
                    if (i > 0 && led_life[j][i-1] == 0)             next_life[j][i-1] = 10; // Propagation vers le haut
                    if (i < Nb_col - 1 && led_life[j][i+1] == 0)    next_life[j][i+1] = 10; // Propagation vers la bas
                }

                // 3. Exinction progressive des LEDs
                if (next_life[j][i] > 0) {
                    next_life[j][i]--;
                }
            } else {
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

// --------------- Pattern Wave / Snake ----------------
void pattern_wave(PIO pio, uint sm, uint len, uint t, uint s){
    int intensite_max = 50;               // Choix de l'intensité max
    int longueur_wave = len/2;              // Choix de la longueur de la wave 
    int milieu_wave = longueur_wave / 2;  // Obtention du milieu de la wave
    int a ; 
  for(uint i = 0; i< len; i++) {  
    if(s){
      a = i + t ;
    } else {
      a = (len - i ) - t ;
    }
    int x = a % (int)len;
    if (x < 0) x += len; // Replacement de x à la fin des leds si il est négatif
    if (x < longueur_wave){ 
      int distance_centre = abs(x - milieu_wave); // Calcul ditance au centre des LEDs
      int r_g;
      if (distance_centre == 0) { 
          r_g = intensite_max; // Intensité maximale au milieu de la wave
      } else {
          r_g = intensite_max / (2*distance_centre); // Pour faible intensité max
      }
      put_pixel(pio, sm, urgb_u32((uint8_t)r_g, r_g, 0));
    } else { 
      put_pixel(pio, sm, urgb_u32(0, 0, 0));
    }
  }
}
// ---------------- Pattern custom ------------------
void pattern_custom_individual(PIO pio, uint sm) {
    for (uint i = 0; i < 20; i++) {
        // Hue = 0 → éteint directement, pas de conversion HSV
        if (led_hues[i] == 0) {
            put_pixel(pio, sm, urgb_u32(0, 0, 0));
        } else {
            put_pixel(pio, sm, hsv_to_urgb(led_hues[i], 255, 10));
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

void update_oled_display() {
  // 1. Lecture de l'ADC
  adc_select_input(1); // Sélectionne l'entrée 1 (GPIO 28)
  uint16_t raw1 = adc_read();

  float current = raw1/(4095.0f * 2.0f); // Calcul de l'intensité 

  ssd1306_clear();
    
  // Affichage du Titre
  ssd1306_draw_string(0, 0, "KEYBOARD");
    
  // Affichage du Mode actuel 
  char mode_str[20];
  switch(potar) {
    case 0: sprintf(mode_str, "RAINBOW"); break;
    case 1: sprintf(mode_str, "WAVE"); break;
    case 2: sprintf(mode_str, "REACTION"); break;
    case 3: sprintf(mode_str, "CUSTOM"); break;
  }
  ssd1306_draw_string(0, 2, mode_str);
    
  // Affichage de la valeur de courant
  char amp_str[20];
  sprintf(amp_str, "CUR: %.2fA", current); // %.2f pour 2 décimales
  ssd1306_draw_string(0, 4, amp_str);

  char lat_str[20];
  sprintf(lat_str, "LAT: %dms", (int)last_latency_ms);
  ssd1306_draw_string(0, 5, lat_str);
    
  ssd1306_flush(); // Activation de l'écran 
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