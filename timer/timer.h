/*================================================================================*
   Pinewood Derby Timer                                Version 3.40 - 24 Jun 2022
   www.dfgtec.com/pdt

   Flexible and affordable Pinewood Derby timer that interfaces with the
   following software:
     - PD Test/Tune/Track Utility
     - Grand Prix Race Manager software

   Refer to the website for setup and usage instructions.


   Copyright (C) 2011-2022 David Gadberry

   This work is licensed under the Creative Commons Attribution-NonCommercial-
   ShareAlike 4.0 International (CC BY-NC-SA 4.0) License. To view a copy of 
   this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/ or 
   send a letter to Creative Commons, 444 Castro Street, Suite 900, 
   Mountain View, California, 94041, USA.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
   POSSIBILITY OF SUCH DAMAGE.
 *================================================================================*/


/*-----------------------------------------*
  - HARDWARE CONFIGURATION -
 *-----------------------------------------*/
//#define LED_DISPLAY  1                 // enable lane place/time displays
//#define MCU_ESP32    1                 // utilize ESP32 MCU 
//#define BT_COMM      1                 // utilize Bluetooth communications (ESP32)

const uint8_t START_TRIP = LOW;        // start switch trip condition
const uint8_t LANE_TRIP  = LOW;       // lane finish trip condition
/*-----------------------------------------*
  - END -
 *-----------------------------------------*/

/*-----------------------------------------*
  - static definitions -
 *-----------------------------------------*/
#define PDT_VERSION  "3.40"            // software version

#ifdef MCU_ESP32
const uint8_t MAX_LANE = 8;            // maximum number of lanes (ESP32)
const int16_t ANLG_MAX = 4095;         // maximum analog range    (ESP32)
#else
const uint8_t MAX_LANE = 6;            //                         (Arduino Uno)
const int16_t ANLG_MAX = 1023;         //                         (Arduino Uno)
#endif

const uint8_t mINIT   = 1;             // timer modes
const uint8_t mREADY  = 2;
const uint8_t mRACING = 3;
const uint8_t mFINISH = 4;
const uint8_t mTEST   = 5;

const float   NULL_TIME = 99.999F;     // null (non-finish) time
const uint8_t NUM_DIGIT = 4;           // timer resolution (# of decimals)

#define char2int(c) (c - '0')          // convert char to integer

/*-----------------------------------------*
  - serial messages -
 *-----------------------------------------*/
//                                        <- to timer
//                                        -> from timer

const char SMSG_CGATE = 'G';           // <- check gate
const char SMSG_GOPEN = 'O';           // -> gate open

const char SMSG_RESET = 'R';           // <- reset
const char SMSG_READY = 'K';           // -> ready

const char SMSG_ACKNW = '.';           // -> acknowledge message
const char SMSG_POWER = 'P';           // -> start-up (power on or hard reset)
const char SMSG_START = 'B';           // -> race started

const char SMSG_SOLEN = 'S';           // <- start solenoid
const char SMSG_FORCE = 'F';           // <- force end
const char SMSG_RSEND = 'Q';           // <- resend race data

const char SMSG_LMASK = 'M';           // <- mask lane
const char SMSG_UMASK = 'U';           // <- unmask all lanes

const char SMSG_GVERS = 'V';           // <- request timer version
const char SMSG_DEBUG = 'D';           // <- toggle debug on/off
const char SMSG_GNUML = 'N';           // <- request number of lanes
const char SMSG_TINFO = 'I';           // <- request timer information

const char SMSG_MPLXR = 'X';           // <- scan I2C multiplexer
const char SMSG_DTEST = 'T';           // <- development test function

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
//                           Lane #   1    2    3    4    5    6    7    8
#ifdef MCU_ESP32
const uint8_t LANE_DET[MAX_LANE] = { 12,  13,  14,  15,  16,  17,  18,  19};    // ESP32
#else
const uint8_t LANE_DET[MAX_LANE] = {  2,   3,   4,   5,   6,   7};              // Arduino Uno
#endif

/*-----------------------------------------*
  - display setup & variables -
 *-----------------------------------------*/
const uint8_t MAX_DISP = 7;            // maximum number of displays

#ifdef LED_DISPLAY
#include "Wire.h"
#include "Adafruit_LEDBackpack.h"      // LED control libraries
#include "Adafruit_GFX.h"

Adafruit_7segment  disp_7seg_1[MAX_DISP], disp_7seg_2[MAX_DISP];
Adafruit_8x8matrix disp_8x8m_1[MAX_DISP], disp_8x8m_2[MAX_DISP];
#endif

// Display I2C addresses
//                        Display #    1     2     3     4     5     6     7
const uint8_t DISP_ADD[MAX_DISP] = {0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76};    // Adafruit displays
//const uint8_t DISP_ADD[MAX_DISP] = {0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77};    // Adafruit displays

// Display messages
const uint8_t msgGateC[] = {0x6D, 0x48, 0x00, 0x39, 0x38};  // S=CL
const uint8_t msgGateO[] = {0x6D, 0x48, 0x00, 0x3F, 0x73};  // S=OP
const uint8_t msgLight[] = {0x48, 0x48, 0x00, 0x38, 0x38};  // ==LL
const uint8_t msgDark [] = {0x48, 0x48, 0x00, 0x5e, 0x5e};  // ==dd
const uint8_t msgDashT[] = {0x40, 0x40, 0x00, 0x40, 0x40};  // ----
const uint8_t msgDashL[] = {0x00, 0x00, 0x00, 0x40, 0x00};  //   -
const uint8_t msgPower[] = {0x00, 0x5c, 0x00, 0x54, 0x00};  //  on 
const uint8_t msgBlank[] = {0x00, 0x00, 0x00, 0x00, 0x00};  // (blank)

const uint8_t msg8x8On[] = {0x18, 0x24, 0x18, 0x00, 0x3C, 0x04, 0x38, 0x00};  // on

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

// display bank addresses (I2C multiplexer)
const uint8_t BANK1 = 0;               // display bank 1 address
const uint8_t BANK2 = 1;               // display bank 2 address
const uint8_t BANK3 = 2;               // display bank 3 address
const uint8_t BANK4 = 3;               // display bank 4 address

const uint8_t I2C_ADDR = 0x77;         // I2C multiplexer address

const uint8_t MIN_BRIGHT =  0;         // minimum display brightness (0-15)
const uint8_t MAX_BRIGHT = 15;         // maximum display brightness (0-15)

const uint8_t DISP_DIGIT = 4;          // total number of display digits

float         display_level = -1.0;    // display brightness level
unsigned long last_disp_update = 0;    // last update (display cycle)

/*-----------------------------------------*
  - status led setup -
 *-----------------------------------------*/
const uint8_t lsOFF = 0;

const uint8_t pON  = 150;              // PWM levels (analogWrite)
const uint8_t pDIM = 220;              //   0 - 100% on, 255 = 0 % on (off)
const uint8_t pOFF = 255;

//                               R     G     B           mode     LED color
const uint8_t LED_STS[6][3] = {{ pOFF, pOFF, pOFF},   // lsOFF    (-off-)
                               { pOFF, pON , pON },   // mINIT    (cyan)
                               { pOFF, pOFF, pON },   // mREADY   (blue)
                               { pOFF, pON , pOFF},   // mRACING  (green)
                               { pON , pOFF, pOFF},   // mFINISH  (red)
                               { pOFF, pDIM, pDIM}};  // mTEST    (dim cyan)

/*-----------------------------------------*
  - communications -
 *-----------------------------------------*/
#ifdef BT_COMM                         // bluetooth
#define SERIAL_COM SerialBT
#include "BluetoothSerial.h"
BluetoothSerial SERIAL_COM;
#else                                  // serial
#define SERIAL_COM Serial
#endif

int           serial_data;             // serial data

/*-----------------------------------------*
  - global variables -
 *-----------------------------------------*/
byte          timer_mode;              // current timer mode
boolean       init_first;              // first pass in init state flag
boolean       ready_first;             // first pass in ready state flag
boolean       finish_first;            // first pass in finish state flag

unsigned long start_time;              // race start time (microseconds)
unsigned long lane_time  [MAX_LANE];   // lane finish time (microseconds)
uint8_t       lane_place [MAX_LANE];   // lane finish place
uint8_t       lane_msk;                // lane mask status
uint8_t       end_cond;                // race end condition

boolean       fDebug = false;          // debug flag

/*-----------------------------------------*
  - function prototypes (default values) -
 *-----------------------------------------*/
void initialize_timer(boolean powerup=false);
void dbg(int, const char * msg, int val=-999);
void smsg(char msg, boolean crlf=true);
void smsg_str(const char * msg, boolean crlf=true);
void update_display(uint8_t pos, const uint8_t msgL[], const uint8_t msgS[]=msgBlank);
void update_display(uint8_t pos, uint8_t place, unsigned long dtime, boolean mode=false);
void clear_displays(const uint8_t msg[]=msgBlank);
void display_msg_7seg(Adafruit_7segment *disp, const uint8_t msg[], boolean flip=false);

