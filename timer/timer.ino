/*================================================================================*
   Pinewood Derby Timer
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

#include "timer.h"

/*-----------------------------------------*
  - TIMER CONFIGURATION -
 *-----------------------------------------*/
const uint8_t NUM_LANES   = 2;         // number of lanes
const boolean GATE_RESET  = false;     // Enable closing start gate to reset timer
const boolean SHOW_PLACE  = true;      // Show place mode
const uint8_t PLACE_DELAY = 3;         // Delay (secs) when displaying place/time

const uint8_t dBANK1 = dt8x8m;
const uint8_t dBANK2 = dt7seg;
/*-----------------------------------------*
  - END -
 *-----------------------------------------*/


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

  for (uint8_t n=0; n<MAX_LANE; n++)
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
  uint8_t finish_order;
  unsigned long current_time, last_finish_time;
  uint32_t lane_sts;
  uint8_t lane_cur, lane_end;
  boolean tfirst=true; //dfg
  uint8_t t=0; //dfg


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

    for (uint8_t n=0; n<NUM_LANES; n++)
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
  TEST PDT FINISH DETECTION HARDWARE
 *================================================================================*/
void test_pdt_hw()
{
  uint8_t lane_status[NUM_LANES];
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

    for (uint8_t n=0; n<NUM_LANES; n++)
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

    for (uint8_t n=0; n<NUM_LANES; n++)
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


  for (uint8_t n=0; n<NUM_LANES; n++)    // send times to computer
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

    for (uint8_t n=0; n<NUM_LANES; n++)
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

  for (uint8_t n=0; n<NUM_LANES; n++)
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
  uint8_t r_lev, b_lev, g_lev;

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
  INITIALIZE TIMER
 *================================================================================*/
void initialize(boolean powerup)
{
  for (uint8_t n=0; n<NUM_LANES; n++)
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

