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
  SEND MESSAGE TO DISPLAY
 *================================================================================*/
void display_msg(uint8_t pos, uint8_t type, const uint8_t msg[])
{
#ifndef LED_DISPLAY
  return;
#else

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

#endif
}


/*================================================================================*
  SEND MESSAGE TO DISPLAY
 *================================================================================*/
void display_place(uint8_t pos, uint8_t type, uint8_t place)
{
#ifndef LED_DISPLAY
  return;
#else

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

#endif
}


/*================================================================================*
  UPDATE LANE PLACE/TIME DISPLAY
 *================================================================================*/
void display_time(uint8_t pos, uint8_t type, unsigned long dtime)
{
#ifndef LED_DISPLAY
  return;
#else

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

#endif
}


/*================================================================================*
  SET LANE DISPLAY BRIGHTNESS
 *================================================================================*/
void set_display_brightness(uint8_t level)
{
#ifndef LED_DISPLAY
  return;
#else

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

#endif
}


/*================================================================================*
  INITIALIZE DISPLAYS
 *================================================================================*/
void display_init()
{
#ifndef LED_DISPLAY
  return;
#else

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

#endif
}


/*================================================================================*
  UPDATE LANE PLACE/TIME DISPLAY
 *================================================================================*/
void update_display(uint8_t pos, const uint8_t msgL[], const uint8_t msgS[])
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

  return;
}


/*================================================================================*
  UPDATE LANE PLACE/TIME DISPLAY
 *================================================================================*/
void update_display(uint8_t pos, uint8_t place, unsigned long dtime, boolean mode)
{
  switch (dBANK1)
  {
    case dt8x8m:
      display_place(pos, dt8x8m, place);
      break;

    case dt7seg:
    case dtL7sg:
      if (mode)
        display_place(pos, dBANK1, place);
      else
        display_time (pos, dBANK1, dtime);
      break;
  }

  switch (dBANK2)
  {
    case dt8x8m:
      display_place(pos+4, dt8x8m, place);
      break;

    case dt7seg:
    case dtL7sg:
      if (mode)
        display_place(pos+4, dBANK2, place);
      else
        display_time (pos+4, dBANK2, dtime);
      break;
      break;
  }

  return;
}


/*================================================================================*
  CLEAR LANE PLACE/TIME DISPLAYS
 *================================================================================*/
void clear_displays(uint8_t const msg[])
{
  dbg(fDebug, "led: CLEAR");

  for (uint8_t n=0; n<NUM_LANES; n++)
  {
    if (timer_mode == mRACING && ((1<<n) & lane_msk))  // don't clear masked lanes
      continue;                                        // at start of race

    update_display(n, msg, msg);
  }

  return;
}


/*================================================================================*
  CYCLE DISPLAYING PLACE & TIME FOR ALL LANES
 *================================================================================*/
void cycle_race_results()
{
  static boolean display_mode;
  unsigned long now;

  if (!SHOW_PLACE || (dBANK1 == dt8x8m || dBANK2 == dt8x8m)) return;

  now = millis();

  if (last_disp_update == 0)  // first cycle
  {
    last_disp_update = now;
    display_mode = false;
  }

  if ((now - last_disp_update) > (unsigned long)(PLACE_DELAY * 1000))
  {
    dbg(fDebug, "cycle_race_results");

    for (uint8_t n=0; n<NUM_LANES; n++)
    {
      if (lane_place[n] != 0)
        update_display(n, lane_place[n], lane_time[n], display_mode);
      else
        update_display(n, msgDashT, msgDashL);
    }

    display_mode = !display_mode;
    last_disp_update = now;
  }

  return;
}


/*================================================================================*
  flip 7 segment bitmask (rotate 180 degrees)
 *================================================================================*/
uint8_t flip_char(uint8_t mc)
{
  return ((B11000000 & mc) + ((B00111000 & mc)>>3) + ((B00000111 & mc)<<3));
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
void set_status_led(uint8_t const ledSTS[])
{
  analogWrite(STATUS_LED_R, ledSTS[0]);
  analogWrite(STATUS_LED_G, ledSTS[1]);
  analogWrite(STATUS_LED_B, ledSTS[2]);

  return;
}

