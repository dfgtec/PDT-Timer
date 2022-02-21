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
  - TIMER CONFIGURATION -
 *-----------------------------------------*/
#define NUM_LANES    2                 // number of lanes
#define GATE_RESET   0                 // Enable closing start gate to reset timer

#define LED_DISPLAY  1                 // Enable lane place/time displays
#define DUAL_DISP    1                 // dual displays per lane (4 lanes max)
//#define DUAL_MODE    1                 // dual display mode
//#define LARGE_DISP   1                 // utilize large Adafruit displays (see website)

#define SHOW_PLACE   1                 // Show place mode
#define PLACE_DELAY  3                 // Delay (secs) when displaying place/time
#define MIN_BRIGHT   0                 // minimum display brightness (0-15)
#define MAX_BRIGHT   15                // maximum display brightness (0-15)

//#define MCU_ESP32    1                 // utilize ESP32 MCU 

const uint8_t dtNONE = 0;
const uint8_t dt8x8m = 1;
const uint8_t dt7seg = 2;
const uint8_t dtL7sg = 3;

const uint8_t dBANK1 = dt8x8m;
const uint8_t dBANK2 = dt7seg;

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
unsigned long tstart, tstop, tdelta, tpoint[10];


/*-----------------------------------------*
  - END -
 *-----------------------------------------*/


#ifdef LED_DISPLAY                     // LED control libraries
#include "Wire.h"
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"
#endif

/*-----------------------------------------*
  - static definitions -
 *-----------------------------------------*/
#define PDT_VERSION  "3.xx"            // software version
#ifdef MCU_ESP32
#define MAX_LANE     8                 // maximum number of lanes (ESP32)
#else
#define MAX_LANE     6                 //                         (Arduino Uno)
#endif
#define MAX_DISP     8                 // maximum number of displays (Adafruit)

#define mREADY       0                 // program modes
#define mRACING      1
#define mFINISH      2
#define mTEST        3

#define START_TRIP   LOW               // start switch trip condition
#define LANE_TRIP    HIGH              // lane finish trip condition
#define NULL_TIME    99.999            // null (non-finish) time
#define NUM_DIGIT    4                 // timer resolution (# of decimals)
#define DISP_DIGIT   4                 // total number of display digits

#define PWM_LED_ON   20
#define PWM_LED_OFF  255
#define char2int(c) (c - '0')

//
// serial messages                        <- to timer
//                                        -> from timer
//
#define SMSG_ACKNW   '.'               // -> acknowledge message

#define SMSG_POWER   'P'               // -> start-up (power on or hard reset)

#define SMSG_CGATE   'G'               // <- check gate
#define SMSG_GOPEN   'O'               // -> gate open

#define SMSG_RESET   'R'               // <- reset
#define SMSG_READY   'K'               // -> ready

#define SMSG_SOLEN   'S'               // <- start solenoid
#define SMSG_START   'B'               // -> race started
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
#ifdef MCU_ESP32             // ESP32
byte BRIGHT_LEV   =  4;                // brightness level
byte RESET_SWITCH =  2;                // reset switch
byte STATUS_LED_R = 25;                // status LED (red)
byte STATUS_LED_B = 27;                // status LED (blue)
byte STATUS_LED_G = 26;                // status LED (green)
byte START_GATE   =  5;                // start gate switch
byte START_SOL    = 23;                // start solenoid
#else                        // Arduino Uno
byte BRIGHT_LEV   = A0;                // brightness level
byte RESET_SWITCH =  8;                // reset switch
byte STATUS_LED_R =  9;                // status LED (red)
byte STATUS_LED_B = 11;                // status LED (blue)
byte STATUS_LED_G = 10;                // status LED (green)
byte START_GATE   = 12;                // start gate switch
byte START_SOL    = 13;                // start solenoid
#endif

//                Display #    1     2     3     4     5     6     7     8
int  DISP_ADD [MAX_DISP] = {0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77};    // display I2C addresses

//                   Lane #    1     2     3     4     5     6     7     8
#ifdef MCU_ESP32
byte LANE_DET [MAX_LANE] = {  12,   13,   14,   15,   16,   17,   18,   19};    // finish detection pins (ESP32)
#else
byte LANE_DET [MAX_LANE] = {   2,    3,    4,    5,    6,    7};                //                       (Arduino Uno)
#endif

/*-----------------------------------------*
  - global variables -
 *-----------------------------------------*/
boolean       fDebug = false;          // debug flag
boolean       ready_first;             // first pass in ready state flag
boolean       finish_first;            // first pass in finish state flag

unsigned long start_time;              // race start time (microseconds)
unsigned long lane_time  [MAX_LANE];   // lane finish time (microseconds)
int           lane_place [MAX_LANE];   // lane finish place
uint8_t       lane_msk;                // lane mask status
uint8_t       end_cond;                // race end condition

int           serial_data;             // serial data
byte          mode;                    // current program mode

float         display_level = -1.0;    // display brightness level

uint8_t msgIndy [] = {0x06, 0x54, 0x00, 0x5E, 0x6E};  // Indy    //dfg
uint8_t msgGateC[] = {0x6D, 0x48, 0x00, 0x39, 0x38};  // S=CL
uint8_t msgGateO[] = {0x6D, 0x48, 0x00, 0x3F, 0x73};  // S=OP
uint8_t msgLight[] = {0x48, 0x48, 0x00, 0x38, 0x38};  // ==LL
uint8_t msgDark [] = {0x48, 0x48, 0x00, 0x5e, 0x5e};  // ==dd
uint8_t msgDashT[] = {0x40, 0x40, 0x00, 0x40, 0x40};  // ----
uint8_t msgDashL[] = {0x00, 0x00, 0x00, 0x40, 0x00};  //   -
uint8_t msgBlank[] = {0x00, 0x00, 0x00, 0x00, 0x00};  // (blank)

#ifdef LED_DISPLAY                     // LED display control
Adafruit_7segment disp_mat[MAX_DISP];
#endif

//#ifdef DUAL_MODE                       // uses 8x8 matrix displays
Adafruit_8x8matrix disp_8x8[MAX_DISP];
//#endif

void initialize(boolean powerup=false);
void dbg(int, const char * msg, int val=-999);
void smsg(char msg, boolean crlf=true);
void smsg_str(const char * msg, boolean crlf=true);
void update_display(uint8_t pos, uint8_t msgL[], uint8_t msgS[]=msgBlank);

/*================================================================================*
  SETUP TIMER
 *================================================================================*/
void setup()
{
/*-----------------------------------------*
  - hardware setup -
 *-----------------------------------------*/
  pinMode(STATUS_LED_R, OUTPUT);
  pinMode(STATUS_LED_B, OUTPUT);
  pinMode(STATUS_LED_G, OUTPUT);
  pinMode(START_SOL,    OUTPUT);
  pinMode(RESET_SWITCH, INPUT_PULLUP);
  pinMode(START_GATE,   INPUT_PULLUP);
  pinMode(BRIGHT_LEV,   INPUT);

  digitalWrite(START_SOL, LOW);

#ifdef MCU_ESP32
  REG_WRITE(GPIO_ENABLE_W1TC_REG, 0xFF << 12);
#endif

  display_init();

  for (int n=0; n<MAX_LANE; n++)
  {
    pinMode(LANE_DET[n], INPUT_PULLUP);
  }
  read_brightness_value();

/*-----------------------------------------*
  - software setup -
 *-----------------------------------------*/
  Serial.begin(9600, SERIAL_8N1);
  smsg(SMSG_POWER);

/*-----------------------------------------*
  - check for test mode -
 *-----------------------------------------*/
  if (digitalRead(RESET_SWITCH) == LOW)
  {
    mode = mTEST;
    test_pdt_hw();
  }

/*-----------------------------------------*
  - initialize timer -
 *-----------------------------------------*/
  initialize(true);
  lane_msk = B00000000;

}


/*================================================================================*
  MAIN LOOP
 *================================================================================*/
void loop()
{
  process_general_msgs();

  switch (mode)
  {
    case mREADY:
      timer_ready_state();
      break;
    case mRACING:
      timer_racing_state();
      break;
    case mFINISH:
      timer_finished_state();
      break;
  }
}


/*================================================================================*
  TIMER READY STATE
 *================================================================================*/
void timer_ready_state()
{
  if (ready_first)
  {
    set_status_led();
    clear_displays();

    ready_first = false;
  }

  if (serial_data == int(SMSG_SOLEN))    // activate start solenoid
  {
    digitalWrite(START_SOL, HIGH);
    smsg(SMSG_ACKNW);
  }

  if (digitalRead(START_GATE) == START_TRIP)    // timer start
  {
    start_time = micros();

    digitalWrite(START_SOL, LOW);

    smsg(SMSG_START);
    delay(100);

    mode = mRACING;
  }

  return;
}


/*================================================================================*
  TIMER RACING STATE
 *================================================================================*/
void timer_racing_state()
{
  int finish_order;
  unsigned long current_time, last_finish_time;
  uint32_t lane_sts;
  uint8_t lane_cur, lane_end;
  boolean tfirst=true; //dfg
  int t=0; //dfg


  set_status_led();
  clear_displays();

  finish_order = 0;
  last_finish_time = 0;
  lane_end = lane_msk;

  while (lane_end < end_cond)
  {
    if (tfirst)
    {
      test_point(t++);

      if (t > 5)
      {
        tfirst = false;
        test_point_print();
      }
    }

    current_time = micros();
#ifdef MCU_ESP32
    lane_sts = REG_READ(GPIO_IN_REG) >> 12;    // read lane status (ESP32)
#else
    lane_sts = PIND >> 2;                      //                  (Arduino Uno)
#endif
    if (!LANE_TRIP) lane_sts = ~lane_sts;      // flip status bits if configured with LANE_TRIP = LOW

    for (int n=0; n<NUM_LANES; n++)
    {
      lane_cur = 1 << n;

      if (!(lane_cur & lane_end) && (lane_cur & lane_sts))    // car has crossed finish line
      {
        lane_end |= lane_cur;

        lane_time[n] = current_time - start_time;
        if (lane_time[n] > last_finish_time)
        {
          finish_order++;
          last_finish_time = lane_time[n];
        }
        lane_place[n] = finish_order;
        update_display(n, lane_place[n], lane_time[n]);
      }
    }

    serial_data = get_serial_data();
    if (serial_data == int(SMSG_FORCE) || serial_data == int(SMSG_RESET) || digitalRead(RESET_SWITCH) == LOW)    // force race to end
    {
      lane_end = end_cond;
      smsg(SMSG_ACKNW);
    }
  }
  send_race_results();
  mode = mFINISH;

  return;
}


/*================================================================================*
  TIMER FINISHED STATE
 *================================================================================*/
void timer_finished_state()
{
  if (finish_first)
  {
    set_status_led();
    finish_first = false;
  }

  if (GATE_RESET && digitalRead(START_GATE) != START_TRIP)    // gate closed
  {
    delay(500);    // ignore any switch bounce

    if (digitalRead(START_GATE) != START_TRIP)    // gate still closed
    {
      initialize();    // reset timer
    }
  }

  if (serial_data == int(SMSG_RSEND))    // resend race data
  {
    smsg(SMSG_ACKNW);
    send_race_results();
  }

  read_brightness_value();
//  display_race_results();

  return;
}


/*================================================================================*
  PROCESS GENERAL SERIAL MESSAGES
 *================================================================================*/
void process_general_msgs()
{
  int lane;
  char tmps[50];


  serial_data = get_serial_data();

  if (serial_data == int(SMSG_GVERS))         // get software version
  {
    sprintf(tmps, "vert=%s", PDT_VERSION);
    smsg_str(tmps);
  }

  else if (serial_data == int(SMSG_GNUML))    // get number of lanes
  {
    sprintf(tmps, "numl=%d", NUM_LANES);
    smsg_str(tmps);
  }

  else if (serial_data == int(SMSG_TINFO))    // get timer information
  {
    send_timer_info();
  }

  else if (serial_data == int(SMSG_DTEST))    // development test function
  {
//dfg
    test_start();
    update_display(0, 2, 1234567);
    test_stop();

    test_start();
    update_display(1, 8, 1234567);
    test_stop();
    delay(3000);

    test_start();
    update_display(0, msgIndy);
    test_stop();

    delay(5000);



#ifdef MCU_ESP32
    uint32_t input = REG_READ(GPIO_IN_REG) >> 12;   // ESP32
#else
    uint8_t input = PIND >> 2;                      // Arduino Uno
#endif
    if (!LANE_TRIP) input = ~input;      // flip status bits if configured with LANE_TRIP = LOW
    Serial.println((uint8_t)input, BIN);
  }

  else if (serial_data == int(SMSG_DEBUG))    // toggle debug
  {
    fDebug = !fDebug;
    dbg(true, "toggle debug = ", fDebug);
  }

  else if (serial_data == int(SMSG_CGATE))    // check start gate
  {
    if (digitalRead(START_GATE) == START_TRIP)    // gate open
    {
      smsg(SMSG_GOPEN);
    }
    else
    {
      smsg(SMSG_ACKNW);
    }
  }

  else if (serial_data == int(SMSG_RESET) || digitalRead(RESET_SWITCH) == LOW)    // timer reset
  {
    if (digitalRead(START_GATE) != START_TRIP)    // only reset if gate closed
    {
      initialize();
    }
    else
    {
      smsg(SMSG_GOPEN);
    }
  }

  else if (serial_data == int(SMSG_LMASK))    // lane mask
  {
    delay(100);
    serial_data = get_serial_data();

    lane = serial_data - 48;
    if (lane >= 1 && lane <= NUM_LANES)
    {
      lane_msk |= 1 << (lane-1);

      dbg(fDebug, "set mask on lane = ", lane);
    }
    smsg(SMSG_ACKNW);
  }

  else if (serial_data == int(SMSG_UMASK))    // unmask all lanes
  {
    lane_msk = B00000000;
    smsg(SMSG_ACKNW);
  }

  return;
}

//dfg
void test_start()
{
    tstart = micros();
}
void test_stop()
{
    tstop = micros();
    tdelta = tstop-tstart;
    Serial.print("d time: ");
    Serial.println(tdelta, 8);
}
void test_point(int cnt)
{
    tpoint[cnt] = micros();
}

void test_point_print()
{
    Serial.println("point times: ");
    for (int n=0; n<5; n++)
    {
      if (n==0)
      {
        Serial.print("   ");
        Serial.println(tpoint[n], 8);
      }
      else
      {
        tstop=tpoint[n]-tpoint[n-1];
        Serial.print("   ");
        Serial.print(tpoint[n], 8);
        Serial.print("   ");
        Serial.println(tstop, 8);
      }
    }
}

/*================================================================================*
  TEST PDT FINISH DETECTION HARDWARE
 *================================================================================*/
void test_pdt_hw()
{
  int lane_status[NUM_LANES];
  char ctmp[10];


  smsg_str("TEST MODE");
  set_status_led();
  delay(2000);

/*-----------------------------------------*
   show status of lane detectors
 *-----------------------------------------*/
  while(true)
  {
    read_brightness_value();

    for (int n=0; n<NUM_LANES; n++)
    {
      lane_status[n] =  digitalRead(LANE_DET[n]);    // read status of all lanes

      if (lane_status[n] == LANE_TRIP)
      {
        update_display(n, msgDark);
      }
      else
      {
        update_display(n, msgLight);
      }
    }

    if (digitalRead(RESET_SWITCH) == LOW)  // break out of this test
    {
      clear_displays();
      delay(1000);
      break;
    }
    delay(100);
  }

/*-----------------------------------------*
   show status of start gate switch
 *-----------------------------------------*/
  while(true)
  {
    read_brightness_value();

    if (digitalRead(START_GATE) == START_TRIP)
    {
      update_display(0, msgGateO);
    }
    else
    {
      update_display(0, msgGateC);
    }

    if (digitalRead(RESET_SWITCH) == LOW)  // break out of this test
    {
      clear_displays();
      delay(1000);
      break;
    }
    delay(100);
  }

/*-----------------------------------------*
   show pattern for brightness adjustment
 *-----------------------------------------*/
  while(true)
  {
    read_brightness_value();

    for (int n=0; n<NUM_LANES; n++)
    {
      unsigned long ttime = (unsigned long)display_level*1000000+(unsigned long)display_level*10000;
      update_display(n, n+1, ttime);
    }

    if (digitalRead(RESET_SWITCH) == LOW)  // break out of this test
    {
      clear_displays();
      break;
    }
    delay(1000);
  }
}


/*================================================================================*
  SEND RACE RESULTS TO COMPUTER
 *================================================================================*/
void send_race_results()
{
  float lane_time_sec;


  for (int n=0; n<NUM_LANES; n++)    // send times to computer
  {
    lane_time_sec = (float)(lane_time[n] / 1000000.0);    // elapsed time (seconds)

    if (lane_time_sec == 0)    // did not finish
    {
      lane_time_sec = NULL_TIME;
    }

    Serial.print(n+1);
    Serial.print(" - ");
    Serial.println(lane_time_sec, NUM_DIGIT);  // numbers are rounded to NUM_DIGIT
                                               // digits by println function
  }

  return;
}


/*================================================================================*
  RACE FINISHED - DISPLAY PLACE / TIME FOR ALL LANES
 *================================================================================*/
void display_race_results()
{
  unsigned long now;
  static boolean display_mode;
  static unsigned long last_display_update = 0;


  if (!SHOW_PLACE) return;

  now = millis();

  if (last_display_update == 0)  // first cycle
  {
    last_display_update = now;
    display_mode = false;
  }

  if ((now - last_display_update) > (unsigned long)(PLACE_DELAY * 1000))
  {
    dbg(fDebug, "display_race_results");

    for (int n=0; n<NUM_LANES; n++)
    {
      update_display(n, lane_place[n], lane_time[n]);
    }

    display_mode = !display_mode;
    last_display_update = now;
  }

  return;
}


/*================================================================================*
  CLEAR LANE PLACE/TIME DISPLAYS
 *================================================================================*/
void clear_displays()
{
  dbg(fDebug, "led: CLEAR");

  for (int n=0; n<NUM_LANES; n++)
  {
    if (mode == mRACING || mode == mTEST)
    {
      update_display(n, msgBlank);           // racing
    }
    else
    {
      update_display(n, msgDashT, msgDashT); // ready
    }
  }

  return;
}


/*================================================================================*
  SET LANE DISPLAY BRIGHTNESS
 *================================================================================*/
void read_brightness_value()
{
#ifndef LED_DISPLAY
  return;
#endif
  float new_level;

  new_level = long(1023 - analogRead(BRIGHT_LEV)) / 1023.0F * 15.0F;
  new_level = constrain(new_level, (float)MIN_BRIGHT, (float)MAX_BRIGHT);

  if (fabs(new_level - display_level) > 0.3F)    // deadband to prevent flickering
  {                                              // between levels
    display_level = new_level;
    set_display_brightness(display_level);
  }

  return;
}


/*================================================================================*
  SET TIMER STATUS LED
 *================================================================================*/
void set_status_led()
{
  int r_lev, b_lev, g_lev;

  dbg(fDebug, "status led = ", mode);

  r_lev = PWM_LED_OFF;
  b_lev = PWM_LED_OFF;
  g_lev = PWM_LED_OFF;

  if (mode == mREADY)         // blue
  {
    b_lev = PWM_LED_ON;
  }
  else if (mode == mRACING)  // green
  {
    g_lev = PWM_LED_ON;
  }
  else if (mode == mFINISH)  // red
  {
    r_lev = PWM_LED_ON;
  }
  else if (mode == mTEST)    // yellow
  {
    r_lev = PWM_LED_ON;
    g_lev = PWM_LED_ON;
  }

  analogWrite(STATUS_LED_R,  r_lev);
  analogWrite(STATUS_LED_B,  b_lev);
  analogWrite(STATUS_LED_G,  g_lev);

  return;
}


/*================================================================================*
  READ SERIAL DATA FROM COMPUTER
 *================================================================================*/
int get_serial_data()
{
  int data = 0;

  if (Serial.available() > 0)
  {
    data = Serial.read();
    dbg(fDebug, "ser rec = ", data);
  }

  return data;
}


/*================================================================================*
  INITIALIZE TIMER
 *================================================================================*/
void initialize(boolean powerup)
{
  for (int n=0; n<NUM_LANES; n++)
  {
    lane_time[n] = 0;
    lane_place[n] = 0;
  }
  end_cond = (1<<NUM_LANES)-1;

  start_time = 0;
  set_status_led();
  digitalWrite(START_SOL, LOW);

  // if power up and gate is open -> goto FINISH state
  if (powerup && digitalRead(START_GATE) == START_TRIP)
  {
    mode = mFINISH;
  }
  else
  {
    mode = mREADY;

    smsg(SMSG_READY);
    delay(100);
  }
  Serial.flush();

  ready_first  = true;
  finish_first  = true;

  return;
}


/*================================================================================*
  SEND DEBUG TO COMPUTER
 *================================================================================*/
void dbg(int flag, const char * msg, int val)
{
  char tmps[50];


  if (!flag) return;

  smsg_str("dbg: ", false);
  smsg_str(msg, false);

  if (val != -999)
  {
    sprintf(tmps, "%d", val);
    smsg_str(tmps);
  }
  else
  {
    smsg_str("");
  }

  return;
}


/*================================================================================*
  SEND SERIAL MESSAGE (CHAR) TO COMPUTER
 *================================================================================*/
void smsg(char msg, boolean crlf)
{
  if (crlf)
  {
    Serial.println(msg);
  }
  else
  {
    Serial.print(msg);
  }
  Serial.flush();
  return;
}


/*================================================================================*
  SEND SERIAL MESSAGE (STRING) TO COMPUTER
 *================================================================================*/
void smsg_str(const char * msg, boolean crlf)
{
  if (crlf)
  {
    Serial.println(msg);
  }
  else
  {
    Serial.print(msg);
  }
  Serial.flush();
  return;
}


/*================================================================================*
  SEND TIMER INFORMATION TO COMPUTER
 *================================================================================*/
void send_timer_info()
{
  char tmps[50];

  Serial.println("-----------------------------");
  sprintf(tmps, " PDT            Version %s", PDT_VERSION);
  Serial.println(tmps);
  Serial.println("-----------------------------");

  sprintf(tmps, "  NUM_LANES     %d", NUM_LANES);
  Serial.println(tmps);
  sprintf(tmps, "  GATE_RESET    %d", GATE_RESET);
  Serial.println(tmps);
  sprintf(tmps, "  SHOW_PLACE    %d", SHOW_PLACE);
  Serial.println(tmps);
  sprintf(tmps, "  PLACE_DELAY   %d", PLACE_DELAY);
  Serial.println(tmps);
  sprintf(tmps, "  MIN_BRIGHT    %d", MIN_BRIGHT);
  Serial.println(tmps);
  sprintf(tmps, "  MAX_BRIGHT    %d", MAX_BRIGHT);
  Serial.println(tmps);

  Serial.println("");

#ifdef LED_DISPLAY
  Serial.println("  LED_DISPLAY   1");
#else
  Serial.println("  LED_DISPLAY   0");
#endif

#ifdef DUAL_DISP
  Serial.println("  DUAL_DISP     1");
#else
  Serial.println("  DUAL_DISP     0");
#endif
#ifdef DUAL_MODE
  Serial.println("  DUAL_MODE     1");
#else
  Serial.println("  DUAL_MODE     0");
#endif

#ifdef LARGE_DISP
  Serial.println("  LARGE_DISP    1");
#else
  Serial.println("  LARGE_DISP    0");
#endif

  Serial.println("");

  sprintf(tmps, "  ARDUINO VERS  %04d", ARDUINO);
  Serial.println(tmps);
  sprintf(tmps, "  COMPILE DATE  %s", __DATE__);
  Serial.println(tmps);
  sprintf(tmps, "  COMPILE TIME  %s", __TIME__);
  Serial.println(tmps);

  Serial.println("-----------------------------");
  Serial.flush();
  return;
}


/*================================================================================*/
/*================================================================================*/
/*================================================================================*/
/*================================================================================*/
/*================================================================================*/
/*================================================================================*/
/*================================================================================*/
/*================================================================================*/
/*================================================================================*/
/*================================================================================*/
/*================================================================================*/
/*================================================================================*/
/*================================================================================*/
/*================================================================================*/
/*================================================================================*/
/*================================================================================*/
/*================================================================================*/
/*================================================================================*/
/*================================================================================*/
/*================================================================================*/
/*================================================================================*/
//dfg

/*================================================================================*
  SEND MESSAGE TO DISPLAY
 *================================================================================*/
void display_msg(uint8_t pos, uint8_t type, uint8_t msg[])
{
#ifndef LED_DISPLAY
  return;
#endif

  if (type >= dt7seg)     
    disp_mat[pos].clear();
  else
    disp_8x8[pos].clear();

  for (uint8_t d=0; d<=4; d++)
  {
    if (d == 3 && type == dt8x8m)     
    {
      disp_8x8[pos].setCursor(2, 0);
      if (msg[d] == 0x40)
        disp_8x8[pos].print("-");
      else
        disp_8x8[pos].print(" ");
    }
    else if (type == dt7seg)
    {
      disp_mat[pos].writeDigitRaw(d, msg[d]);
    }
    else if (type == dtL7sg)
    {
      uint8_t rc = flip_char(msg[d]);
      disp_mat[pos].writeDigitRaw(4-d, rc);
    }
  }

  if (type >= dt7seg)     
    disp_mat[pos].writeDisplay();
  else
    disp_8x8[pos].writeDisplay();

  return;
}


/*================================================================================*
  SEND MESSAGE TO DISPLAY
 *================================================================================*/
void display_place(uint8_t pos, uint8_t type, uint8_t place)
{
#ifndef LED_DISPLAY
  return;
#endif
  char cplace[4];

  if (type >= dt7seg)     
    disp_mat[pos].clear();
  else
    disp_8x8[pos].clear();

  if (type == dt8x8m)     
  {
    disp_8x8[pos].setCursor(2, 0);
    sprintf(cplace, "%1d", place);
    disp_8x8[pos].print(cplace[0]);
  }
  else if (type == dt7seg)
  {
    disp_mat[pos].writeDigitNum(3, place, false);
  }
  else if (type == dtL7sg)
  {
    uint8_t rc = flip_char(numMasks[place]);
    disp_mat[pos].writeDigitRaw(1, rc);
  }

  if (type >= dt7seg)     
    disp_mat[pos].writeDisplay();
  else
    disp_8x8[pos].writeDisplay();

  return;
}


/*================================================================================*
  UPDATE LANE PLACE/TIME DISPLAY
 *================================================================================*/
void display_time(uint8_t pos, uint8_t type, unsigned long dtime)
{
#ifndef LED_DISPLAY
  return;
#endif
  char ctime[10];
  boolean showdot=false;

  if (type < dt7seg) return;

  disp_mat[pos].clear();
  disp_mat[pos].drawColon(false);
  if (type == dtL7sg) disp_mat[pos].writeDigitRaw(2, 16);  // ? dot on large display

  // calculate seconds and convert to string
  dtostrf((double)(dtime / (double)1000000.0), (DISP_DIGIT+1), DISP_DIGIT, ctime);

  uint8_t c = 0;
  for (uint8_t d=0; d<DISP_DIGIT; d++)
  {
    uint8_t dignum = char2int(ctime[c]);

    if (type == dt7seg)
    {
      showdot = (ctime[c+1] == '.');
      disp_mat[pos].writeDigitNum(d+(uint8_t)(d/2), dignum, showdot);    // time
    }
    else
    {
      uint8_t rc = flip_char(numMasks[dignum]);
      disp_mat[pos].writeDigitRaw(4-(d+(uint8_t)(d/2)), rc);    // time
    }

    c++; if (ctime[c] == '.') c++;
  }
  disp_mat[pos].writeDisplay();

  return;
}


/*================================================================================*
  SET LANE DISPLAY BRIGHTNESS
 *================================================================================*/
void set_display_brightness(uint8_t level)
{
#ifndef LED_DISPLAY
  return;
#endif

  for (uint8_t n=0; n<NUM_LANES; n++)
  {
    switch (dBANK1)
    {
      case dt8x8m:
        disp_8x8[n].setBrightness(level);
        break;

      case dt7seg:
      case dtL7sg:
        disp_mat[n].setBrightness(level);
        break;
    }
  
    switch (dBANK2)
    {
      case dt8x8m:
        disp_8x8[n+4].setBrightness(level);
        break;
  
    case dt7seg:
    case dtL7sg:
        disp_mat[n+4].setBrightness(level);
        break;
    }
  }
  return;
}


/*================================================================================*
  INITIALIZE DISPLAYS
 *================================================================================*/
void display_init()
{
#ifndef LED_DISPLAY
  return;
#endif

  for (uint8_t n=0; n<MAX_DISP; n++)
  {
    disp_mat[n] = Adafruit_7segment();
    disp_mat[n].begin(DISP_ADD[n]);
    disp_mat[n].clear();
    disp_mat[n].drawColon(false);
    disp_mat[n].writeDisplay();

    disp_8x8[n] = Adafruit_8x8matrix();
    disp_8x8[n].begin(DISP_ADD[n]);
    disp_8x8[n].setTextSize(1);
    disp_8x8[n].setRotation(3);
    disp_8x8[n].clear();
    disp_8x8[n].writeDisplay();
  }

  return;
}


/*================================================================================*
  UPDATE LANE PLACE/TIME DISPLAY
 *================================================================================*/
void update_display(uint8_t pos, uint8_t msgL[], uint8_t msgS[])
{
  switch (dBANK1)
  {
    case dt8x8m:
      display_msg(pos, dt8x8m, msgS);
      break;

    case dt7seg:
    case dtL7sg:
      display_msg(pos, dBANK1, msgL);
      break;
  }

  switch (dBANK2)
  {
    case dt8x8m:
      display_msg(pos+4, dt8x8m, msgS);
      break;

    case dt7seg:
    case dtL7sg:
      display_msg(pos+4, dBANK2, msgL);
      break;
  }

}

/*================================================================================*
  UPDATE LANE PLACE/TIME DISPLAY
 *================================================================================*/
void update_display(uint8_t pos, uint8_t place, unsigned long dtime)
{
  switch (dBANK1)
  {
    case dt8x8m:
      display_place(pos, dt8x8m, place);
      break;

    case dt7seg:
    case dtL7sg:
      display_time(pos, dBANK1, dtime);
      break;
  }

  switch (dBANK2)
  {
    case dt8x8m:
      display_place(pos+4, dt8x8m, place);
      break;

    case dt7seg:
    case dtL7sg:
      display_time(pos+4, dBANK2, dtime);
      break;
  }
}


/*================================================================================*
  flip 7 segment bitmask (rotate 180 degrees)
 *================================================================================*/
uint8_t flip_char(uint8_t mc)
{
  return ((B11000000 & mc) + ((B00111000 & mc)>>3) + ((B00000111 & mc)<<3));
}

