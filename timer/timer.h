/*================================================================================*
   Pinewood Derby Timer                                Version 3.xx - ?? ??? 2022
   www.dfgtec.com/pdt

   Flexible and affordable Pinewood Derby timer that interfaces with the
   following software:
     - PD Test/Tune/Track Utility
     - Grand Prix Race Manager software

   Refer to the website for setup and usage instructions.


   Copyright (C) 2011-2022 David Gadberry

   This work is licensed under the Creative Commons Attribution-NonCommercial-
   ShareAlike 3.0 Unported License. To view a copy of this license, visit
   http://creativecommons.org/licenses/by-nc-sa/3.0/ or send a letter to
   Creative Commons, 444 Castro Street, Suite 900, Mountain View, California,
   94041, USA.
 *================================================================================*/


/*-----------------------------------------*
  - hardware configuration -
 *-----------------------------------------*/
#define LED_DISPLAY  1                 // Enable lane place/time displays
//#define MCU_ESP32    1                 // utilize ESP32 MCU 
const uint8_t START_TRIP = LOW;        // start switch trip condition
const uint8_t LANE_TRIP  = HIGH;       // lane finish trip condition

/*-----------------------------------------*
  - static definitions -
 *-----------------------------------------*/
#define PDT_VERSION  "3.xx"            // software version

#ifdef MCU_ESP32
const uint8_t MAX_LANE = 8;            // maximum number of lanes (ESP32)
#else
const uint8_t MAX_LANE = 6;            //                         (Arduino Uno)
#endif

const uint8_t mREADY   = 0;            // timer modes
const uint8_t mRACING  = 1;
const uint8_t mFINISH  = 2;
const uint8_t mTEST    = 3;

const float   NULL_TIME  = 99.999F;    // null (non-finish) time
const uint8_t NUM_DIGIT  = 4;          // timer resolution (# of decimals)

const uint8_t PWM_LED_ON  =  20;       // status LED config
const uint8_t PWM_LED_OFF = 255;

#define char2int(c) (c - '0')          // char processing function

/*-----------------------------------------*
  - serial messages -
 *-----------------------------------------*/
//                                        <- to timer
//                                        -> from timer

#define SMSG_CGATE   'G'               // <- check gate
#define SMSG_GOPEN   'O'               // -> gate open

#define SMSG_RESET   'R'               // <- reset
#define SMSG_READY   'K'               // -> ready

#define SMSG_ACKNW   '.'               // -> acknowledge message
#define SMSG_POWER   'P'               // -> start-up (power on or hard reset)
#define SMSG_START   'B'               // -> race started

#define SMSG_SOLEN   'S'               // <- start solenoid
#define SMSG_FORCE   'F'               // <- force end
#define SMSG_RSEND   'Q'               // <- resend race data

#define SMSG_LMASK   'M'               // <- mask lane
#define SMSG_UMASK   'U'               // <- unmask all lanes

#define SMSG_GVERS   'V'               // <- request timer version
#define SMSG_DEBUG   'D'               // <- toggle debug on/off
#define SMSG_GNUML   'N'               // <- request number of lanes
#define SMSG_TINFO   'I'               // <- request timer information

#define SMSG_DTEST   'T'               // <- development test function

/*-----------------------------------------*
  - pin assignments -
 *-----------------------------------------*/
#ifdef MCU_ESP32                       // ESP32
const uint8_t BRIGHT_LEV   =  4;           // brightness level
const uint8_t RESET_SWITCH =  2;           // reset switch
const uint8_t STATUS_LED_R = 25;           // status LED (red)
const uint8_t STATUS_LED_B = 27;           // status LED (blue)
const uint8_t STATUS_LED_G = 26;           // status LED (green)
const uint8_t START_GATE   =  5;           // start gate switch
const uint8_t START_SOL    = 23;           // start solenoid
#else                                  // Arduino Uno
const uint8_t BRIGHT_LEV   = A0;           // brightness level
const uint8_t RESET_SWITCH =  8;           // reset switch
const uint8_t STATUS_LED_R =  9;           // status LED (red)
const uint8_t STATUS_LED_B = 11;           // status LED (blue)
const uint8_t STATUS_LED_G = 10;           // status LED (green)
const uint8_t START_GATE   = 12;           // start gate switch
const uint8_t START_SOL    = 13;           // start solenoid
#endif

// Finish detection sensors
//                           Lane #    1     2     3     4     5     6     7     8
#ifdef MCU_ESP32
const uint8_t LANE_DET[MAX_LANE] = {  12,   13,   14,   15,   16,   17,   18,   19};    // ESP32
#else
const uint8_t LANE_DET[MAX_LANE] = {   2,    3,    4,    5,    6,    7};                // Arduino Uno
#endif

/*-----------------------------------------*
  - display setup & variables -
 *-----------------------------------------*/
const uint8_t MAX_DISP = 8;            // maximum number of displays

#ifdef LED_DISPLAY
#include "Wire.h"
#include "Adafruit_LEDBackpack.h"      // LED control libraries
#include "Adafruit_GFX.h"

Adafruit_7segment  disp_mat[MAX_DISP];
Adafruit_8x8matrix disp_8x8[MAX_DISP];
#endif

// Display I2C addresses
//                        Display #    1     2     3     4     5     6     7     8
const uint8_t DISP_ADD[MAX_DISP] = {0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77};    // Adafruit displays

// Display messages
uint8_t msgGateC[] = {0x6D, 0x48, 0x00, 0x39, 0x38};  // S=CL
uint8_t msgGateO[] = {0x6D, 0x48, 0x00, 0x3F, 0x73};  // S=OP
uint8_t msgLight[] = {0x48, 0x48, 0x00, 0x38, 0x38};  // ==LL
uint8_t msgDark [] = {0x48, 0x48, 0x00, 0x5e, 0x5e};  // ==dd
uint8_t msgDashT[] = {0x40, 0x40, 0x00, 0x40, 0x40};  // ----
uint8_t msgDashL[] = {0x00, 0x00, 0x00, 0x40, 0x00};  //   -
uint8_t msgPower[] = {0x00, 0x5c, 0x00, 0x54, 0x00};  // oooo
uint8_t msgBlank[] = {0x00, 0x00, 0x00, 0x00, 0x00};  // (blank)

// Number (0-9) bitmasks
const uint8_t numMasks[] = {
    0b00111111, // 0
    0b00000110, // 1
    0b01011011, // 2
    0b01001111, // 3
    0b01100110, // 4
    0b01101101, // 5
    0b01111101, // 6
    0b00000111, // 7
    0b01111111, // 8
    0b01101111, // 9
};

// display types (per bank)
const uint8_t dtNONE = 0;              // no displays
const uint8_t dt8x8m = 1;              // Adafruit 8x8 matix displays
const uint8_t dt7seg = 2;              // Adafruit 0.56" 7-segment displays
const uint8_t dtL7sg = 3;              // Adafruit 1.20" 7-segment displays (inverted)

const uint8_t MIN_BRIGHT =  0;         // minimum display brightness (0-15)
const uint8_t MAX_BRIGHT = 15;         // maximum display brightness (0-15)

const uint8_t DISP_DIGIT = 4;          // total number of display digits

float         display_level = -1.0;    // display brightness level
unsigned long last_disp_update = 0;    // last update (display cycle)

/*-----------------------------------------*
  - global variables -
 *-----------------------------------------*/
byte          mode;                    // current program mode
boolean       ready_first;             // first pass in ready state flag
boolean       finish_first;            // first pass in finish state flag

unsigned long start_time;              // race start time (microseconds)
unsigned long lane_time  [MAX_LANE];   // lane finish time (microseconds)
uint8_t       lane_place [MAX_LANE];   // lane finish place
uint8_t       lane_msk;                // lane mask status
uint8_t       end_cond;                // race end condition

boolean       fDebug = false;          // debug flag
int           serial_data;             // serial data

/*-----------------------------------------*
  - function prototypes (default values) -
 *-----------------------------------------*/
void initialize_timer(boolean powerup=false);
void dbg(int, const char * msg, int val=-999);
void smsg(char msg, boolean crlf=true);
void smsg_str(const char * msg, boolean crlf=true);
void update_display(uint8_t pos, uint8_t msgL[], uint8_t msgS[]=msgBlank);
void update_display(uint8_t pos, uint8_t place, unsigned long dtime, boolean mode=false);
void clear_displays(uint8_t msg[]=msgBlank);


// dfg TEST variables
unsigned long tstart, tstop, tdelta, tpoint[10];
uint8_t msgIndy [] = {0x06, 0x54, 0x00, 0x5E, 0x6E};
