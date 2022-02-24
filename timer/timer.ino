/*================================================================================*
   Pinewood Derby Timer
   www.dfgtec.com/pdt

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

#include "timer.h"

/*-----------------------------------------*
  - TIMER CONFIGURATION -
 *-----------------------------------------*/
const uint8_t NUM_LANES   = 2;         // number of lanes
const boolean GATE_RESET  = false;     // Enable closing start gate to reset timer
const boolean SHOW_PLACE  = true;      // Show place mode
const uint8_t PLACE_DELAY = 3;         // Delay (secs) when displaying place/time

const uint8_t dBANK1 = dt8x8m;         // display type of first bank of displays
const uint8_t dBANK2 = dt7seg;         // display type of second bank of displays
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

  for (uint8_t n=0; n<MAX_LANE; n++)
  {
    pinMode(LANE_DET[n], INPUT_PULLUP);
  }

  digitalWrite(START_SOL, LOW);

#ifdef MCU_ESP32
  REG_WRITE(GPIO_ENABLE_W1TC_REG, 0xFF << 12);
#endif

  display_init();
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
    test_timer_hw();
  }

/*-----------------------------------------*
  - initialize timer -
 *-----------------------------------------*/
  initialize_timer(true);
  lane_msk = B00000000;

  return;
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
    clear_displays(msgDashT);

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
  uint8_t finish_order, lane_cur, lane_end;
  unsigned long current_time, last_finish_time;
  uint32_t lane_sts;

  set_status_led();
  clear_displays();

  finish_order = 0;
  last_finish_time = 0;
  lane_end = lane_msk;

  while (lane_end < end_cond)
  {
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
      initialize_timer();    // reset timer
    }
  }

  if (serial_data == int(SMSG_RSEND))    // resend race data
  {
    smsg(SMSG_ACKNW);
    send_race_results();
  }

  read_brightness_value();
  cycle_race_results();

  return;
}


/*================================================================================*
  TEST PDT FINISH DETECTION HARDWARE
 *================================================================================*/
void test_timer_hw()
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
  INITIALIZE TIMER
 *================================================================================*/
void initialize_timer(boolean powerup)
{
  for (uint8_t n=0; n<NUM_LANES; n++)
  {
    lane_time [n] = 0;
    lane_place[n] = 0;
  }
  end_cond = (1<<NUM_LANES)-1;

  start_time = 0;
  set_status_led();
  digitalWrite(START_SOL, LOW);

  // if power up and gate is open -> goto FINISH state
  if (powerup && digitalRead(START_GATE) == START_TRIP)
  {
    clear_displays(msgPower);
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
  finish_first = true;

  return;
}

