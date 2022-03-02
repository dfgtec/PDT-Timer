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
  char tmps[15];


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

  else if (serial_data == int(SMSG_DEBUG))    // toggle debug
  {
    fDebug = !fDebug;
    dbg(true, "toggle debug = ", fDebug);
  }

  else if (serial_data == int(SMSG_CGATE))    // check start gate
  {
    if (digitalRead(START_GATE) == START_TRIP)
    {
      smsg(SMSG_GOPEN);   // gate open
    }
    else
    {
      smsg(SMSG_ACKNW);   // OK (closed)
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
      smsg(SMSG_GOPEN);   // gate open
      delay(100);
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

  else if (serial_data == int(SMSG_DTEST))    // development test function
  {
#ifdef MCU_ESP32
    uint32_t input = REG_READ(GPIO_IN_REG) >> 12;   // ESP32
#else
    uint8_t input = PIND >> 2;                      // Arduino Uno
#endif
    if (!LANE_TRIP) input = ~input;      // flip status bits if configured with LANE_TRIP = LOW
    SERIAL_COM.println((uint8_t)input, BIN);
  }

  return;
}


/*================================================================================*
  READ SERIAL DATA FROM COMPUTER
 *================================================================================*/
int get_serial_data()
{
  int data = 0;

  if (SERIAL_COM.available() > 0)
  {
    data = SERIAL_COM.read();
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
    SERIAL_COM.println(msg);
  }
  else
  {
    SERIAL_COM.print(msg);
  }
  return;
}


/*================================================================================*
  SEND SERIAL MESSAGE (STRING) TO COMPUTER
 *================================================================================*/
void smsg_str(const char * msg, boolean crlf)
{
  if (crlf)
  {
    SERIAL_COM.println(msg);
  }
  else
  {
    SERIAL_COM.print(msg);
  }
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
      update_display(n, msgDashT, msgDashL);
      lane_time_sec = NULL_TIME;
    }

    SERIAL_COM.print(n+1);
    SERIAL_COM.print(" - ");
    SERIAL_COM.println(lane_time_sec, NUM_DIGIT);  // numbers are rounded to NUM_DIGIT
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

  SERIAL_COM.println("-----------------------------");
  sprintf(tmps, " PDT            Version %s", PDT_VERSION);
  SERIAL_COM.println(tmps);
  SERIAL_COM.println("-----------------------------");

  sprintf(tmps, "  NUM_LANES     %d", NUM_LANES);
  SERIAL_COM.println(tmps);
  sprintf(tmps, "  GATE_RESET    %d", GATE_RESET);
  SERIAL_COM.println(tmps);
  sprintf(tmps, "  SHOW_PLACE    %d", SHOW_PLACE);
  SERIAL_COM.println(tmps);
  sprintf(tmps, "  PLACE_DELAY   %d", PLACE_DELAY);
  SERIAL_COM.println(tmps);

  SERIAL_COM.println("");

#ifdef LED_DISPLAY
  SERIAL_COM.println("  LED_DISPLAY   1");
#else
  SERIAL_COM.println("  LED_DISPLAY   0");
#endif
  sprintf(tmps, "  dBANK1        %d", dBANK1);
  SERIAL_COM.println(tmps);
  sprintf(tmps, "  dBANK2        %d", dBANK2);
  SERIAL_COM.println(tmps);
  sprintf(tmps, "  brightness    %d", (uint8_t)display_level);
  SERIAL_COM.println(tmps);
  sprintf(tmps, "  MIN_BRIGHT    %d", MIN_BRIGHT);
  SERIAL_COM.println(tmps);
  sprintf(tmps, "  MAX_BRIGHT    %d", MAX_BRIGHT);
  SERIAL_COM.println(tmps);

  SERIAL_COM.println("");

#ifdef MCU_ESP32
  SERIAL_COM.println("  MCU_ESP32     1");
#else
  SERIAL_COM.println("  MCU_ESP32     0");
#endif
  sprintf(tmps, "  MAX_LANE      %d", MAX_LANE);
  SERIAL_COM.println(tmps);

  SERIAL_COM.println("");

if (START_TRIP)
  SERIAL_COM.println("  START_TRIP    HIGH");
else
  SERIAL_COM.println("  START_TRIP    LOW");

if (LANE_TRIP)
  SERIAL_COM.println("  LANE_TRIP     HIGH");
else
  SERIAL_COM.println("  LANE_TRIP     LOW");

  SERIAL_COM.println("");

  sprintf(tmps, "  ARDUINO VERS  %04d", ARDUINO);
  SERIAL_COM.println(tmps);
  sprintf(tmps, "  COMPILE DATE  %s", __DATE__);
  SERIAL_COM.println(tmps);
  sprintf(tmps, "  COMPILE TIME  %s", __TIME__);
  SERIAL_COM.println(tmps);

  SERIAL_COM.println("-----------------------------");
  SERIAL_COM.flush();

  return;
}

