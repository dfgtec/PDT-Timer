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


/*================================================================================*
  PROCESS GENERAL SERIAL MESSAGES
 *================================================================================*/
void process_general_msgs()
{
  uint8_t lane;
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
      initialize_timer();
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
void test_point(uint8_t cnt)
{
    tpoint[cnt] = micros();
}

void test_point_print()
{
    Serial.println("point times: ");
    for (uint8_t n=0; n<5; n++)
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



