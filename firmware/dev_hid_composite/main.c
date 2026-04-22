#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"

#include "bsp/board_api.h"
#include "tusb.h"

#include "usb_descriptors.h"


#define Nb_col 4
#define Nb_ligne 5

#define WS2812_PIN 28
#define NUM_PIXELS 10

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
static uint32_t blink_led = 10; 

void led_blinking_task(void);
void hid_task(void);
void led_blinking_task_2(PIO pio, uint sm); 

//-------------------------------------------------------------------+
// KEYBOARD MATRIX
//-------------------------------------------------------------------+


const uint row_pins[] = {5,
                        6,
                        7,
                        8,
                        9};
const uint col_pins[] = {10,11,12,13};
// Mapping des touche
const const uint8_t hid_keys[Nb_ligne][Nb_col] = {
    {HID_KEY_A, HID_KEY_B,HID_KEY_G,HID_KEY_H},
    {HID_KEY_C, HID_KEY_D, HID_KEY_I, HID_KEY_J},
    {HID_KEY_SHIFT_LEFT, HID_KEY_CONTROL_RIGHT,HID_KEY_K,HID_KEY_L},
    {HID_KEY_M, HID_KEY_N, HID_KEY_O, HID_KEY_P},
    {HID_KEY_Q, HID_KEY_R, HID_KEY_S, HID_KEY_T}
};

int keys_pressed[Nb_ligne][Nb_col] = {
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

int Nb_temps_long = 500; //Temps avant d'être dans l'état : Appuie long
int Compteur_temps_long = 5; //Compteur une fois dans l'état : Appuie long

bool repeating_timer_callback(struct repeating_timer *t) {
    for(int init = 0; init < Nb_col; init++){
      gpio_put(col_pins[init], 0);
    }
    for(int col = 0; col < Nb_col; col++){
      gpio_put(col_pins[col], 1);
      for (int row = 0; row < Nb_ligne; row++) {
          if (gpio_get(row_pins[row])&&(keys_pressed[row][col]==0)){
              keys_to_send[row][col] = 1 ; 
              //printf("%c\n", keys[row][col]);
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

void pico_init(); 
/*------------- MAIN -------------*/
int main(void)
{
  board_init();
  stdio_init_all();
  pico_init(); 
  //id_keys_init();
  struct repeating_timer timer;
  add_repeating_timer_us(-1000, repeating_timer_callback, NULL, &timer);

  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

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
  while (1)
  {
    tud_task(); // tinyusb device task
    led_blinking_task();
    led_blinking_task_2(pio, sm); 
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
          //if (has_keyboard_key) tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
      has_keyboard_key = false;
    }
    break;
    default: break;
  }

}

void hid_task(void)
{
  // Poll every 10ms
  const uint32_t interval_ms = 10;
  static uint32_t start_ms = 0;

  if ( board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;
  
  send_hid_report(REPORT_ID_KEYBOARD);
  
}

// // Invoked when sent REPORT successfully to host
// // Application can use this to send the next report
// // Note: For composite reports, report[0] is report ID
// void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
// {
//   (void) instance;
//   (void) len;
//   uint8_t next_report_id = report[0] + 1u;
//   if (next_report_id < REPORT_ID_COUNT)
//   {
//     send_hid_report(next_report_id, board_button_read());
//   }
// }

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

void led_blinking_task_2(PIO pio, uint sm)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // blink is disabled
  if (!blink_led) return;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_led) return; // not enough time
  start_ms += blink_led;

  if (led_state){
    int i = 50; 
        put_pixel(pio,sm,urgb_u32(i, 0, 0));
        put_pixel(pio,sm,urgb_u32(0, i, 0));
        put_pixel(pio,sm,urgb_u32(0, 0, i));
        put_pixel(pio,sm,urgb_u32(i, 0, 0));
        put_pixel(pio,sm,urgb_u32(0, i, 0));
        put_pixel(pio,sm,urgb_u32(0, 0, i));
        put_pixel(pio,sm,urgb_u32(i, 0, 0));
        put_pixel(pio,sm,urgb_u32(0, i, 0));
        put_pixel(pio,sm,urgb_u32(0, 0, i));
        put_pixel(pio,sm,urgb_u32(50, 50, 50));
        //
        put_pixel(pio,sm,urgb_u32(0, 0, 0));
        put_pixel(pio,sm,urgb_u32(0, i, 0));
        put_pixel(pio,sm,urgb_u32(0, 0, i));
        put_pixel(pio,sm,urgb_u32(i, 0, 0));
        put_pixel(pio,sm,urgb_u32(0, i, 0));
        put_pixel(pio,sm,urgb_u32(0, 0, i));
        put_pixel(pio,sm,urgb_u32(i, 0, 0));
        put_pixel(pio,sm,urgb_u32(0, i, 0));
        put_pixel(pio,sm,urgb_u32(0, 0, i));
        put_pixel(pio,sm,urgb_u32(50, 50, 50));
  }
  else {
    for (int i = 0; i < PIX)
  }
  led_state = 1 - led_state; // toggle
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
    gpio_init(18);
    gpio_init(19);
    gpio_set_dir(18, GPIO_OUT);
    gpio_set_dir(19, GPIO_OUT);
}