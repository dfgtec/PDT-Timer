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
  SEND MESSAGE TO 8x8 MATRIX DISPLAY
 *================================================================================*/
void display_msg_8x8m(Adafruit_8x8matrix *disp, const uint8_t msg[])
{
#ifndef LED_DISPLAY
  return;
#else

  disp->clear();
  disp->setCursor(2, 0);

  if (msg[3] == 0x40)
    disp->print("-");

  disp->writeDisplay();

  return;

#endif
}


/*================================================================================*
  SEND MESSAGE TO 7 SEGMENT DISPLAY
 *================================================================================*/
void display_msg_7seg(Adafruit_7segment *disp, const uint8_t msg[], boolean flip)
{
#ifndef LED_DISPLAY
  return;
#else
  uint8_t pos, dch;

  disp->clear();

  for (uint8_t d=0; d<=4; d++)
  {
    pos = d; dch = msg[d];

    if (flip)  // invert and reverse for large 7 segment displays
    {
      dch = flip_char(msg[d]);
      pos = 4 - d;
    }

    disp->writeDigitRaw(pos, dch);
  }
  disp->writeDisplay();

  return;

#endif
}


/*================================================================================*
  SEND PLACE TO 8x8 MATRIX DISPLAY
 *================================================================================*/
void display_place_8x8m(Adafruit_8x8matrix *disp, uint8_t place)
{
#ifndef LED_DISPLAY
  return;
#else

  char cplace[4];

  disp->clear();
  disp->setCursor(2, 0);

  sprintf(cplace, "%1d", place);
  disp->print(cplace[0]);

  disp->writeDisplay();

  return;

#endif
}


/*================================================================================*
  SEND PLACE TO 7 SEGMENT DISPLAY
 *================================================================================*/
void display_place_7seg(Adafruit_7segment *disp, uint8_t place, boolean flip)
{
#ifndef LED_DISPLAY
  return;
#else
  uint8_t pos, dch;

  disp->clear();

  pos = 3; dch = numMasks[place];

  if (flip)  // invert and reverse for large 7 segment displays
  {
    pos = 1;
    dch = flip_char(numMasks[place]);
  }

  disp->writeDigitRaw(pos, dch);
  disp->writeDisplay();

  return;

#endif
}


/*================================================================================*
  UPDATE LANE PLACE/TIME DISPLAY
 *================================================================================*/
void display_time_7seg(Adafruit_7segment *disp, unsigned long dtime, boolean flip)
{
#ifndef LED_DISPLAY
  return;
#else

  char ctime[10];
  boolean showdot=false;

  disp->clear();
  disp->drawColon(false);
  if (flip) disp->writeDigitRaw(2, 16);  // ? dot on large display

  // calculate seconds and convert to string
  dtostrf((double)(dtime / (double)1000000.0), (DISP_DIGIT+1), DISP_DIGIT, ctime);

  uint8_t c = 0;
  for (uint8_t d=0; d<DISP_DIGIT; d++)
  {
    uint8_t dignum = char2int(ctime[c]);

    if (flip)
    {
      uint8_t rc = flip_char(numMasks[dignum]);
      disp->writeDigitRaw(4-(d+(uint8_t)(d/2)), rc);    // time
    }
    else
    {
      showdot = (ctime[c+1] == '.');
      disp->writeDigitNum(d+(uint8_t)(d/2), dignum, showdot);    // time
    }

    c++; if (ctime[c] == '.') c++;
  }
  disp->writeDisplay();

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

  if (dtBANK1 == dt7seg || dtBANK1 == dtL7sg)   // front main display bank (time or place/time)
  {
    set_display_bank(BANK1);
    for (uint8_t n=0; n<NUM_LANES; n++)
      disp_7seg_1[n].setBrightness(level);
  }

  if (dtBANK2 == dt8x8m)                       // front place display bank (place)
  {
    set_display_bank(BANK2);
    for (uint8_t n=0; n<NUM_LANES; n++)
      disp_8x8m_1[n].setBrightness(level);
  }

  if (dtBANK3 == dt7seg || dtBANK3 == dtL7sg)   // rear main display bank (time or place/time)
  {
    set_display_bank(BANK3);
    for (uint8_t n=0; n<NUM_LANES; n++)
      disp_7seg_2[n].setBrightness(level);
  }

  if (dtBANK4 == dt8x8m)                       // rear place display bank (place)
  {
    set_display_bank(BANK4);
    for (uint8_t n=0; n<NUM_LANES; n++)
      disp_8x8m_2[n].setBrightness(level);
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
  Wire.begin();

  if (dtBANK1)                                  // front display bank (time or place/time)
  {
    set_display_bank(BANK1);

    for (uint8_t n=0; n<NUM_LANES; n++)
    {
      disp_7seg_1[n] = Adafruit_7segment();
      disp_7seg_1[n].begin(DISP_ADD[n]);
      disp_7seg_1[n].clear();
      disp_7seg_1[n].drawColon(false);
      disp_7seg_1[n].writeDisplay();
    }
  }

  if (dtBANK2)                                 // front display bank (place)
  {
    set_display_bank(BANK2);

    for (uint8_t n=0; n<NUM_LANES; n++)
    {
      disp_8x8m_1[n] = Adafruit_8x8matrix();
      disp_8x8m_1[n].begin(DISP_ADD[n]);
      disp_8x8m_1[n].setTextSize(1);
      disp_8x8m_1[n].setRotation(3);
      disp_8x8m_1[n].clear();
      disp_8x8m_1[n].writeDisplay();
    }
  }

  if (dtBANK3)                                  // rear display bank (time or place/time)
  {
    set_display_bank(BANK3);

    for (uint8_t n=0; n<NUM_LANES; n++)
    {
      disp_7seg_2[n] = Adafruit_7segment();
      disp_7seg_2[n].begin(DISP_ADD[n]);
      disp_7seg_2[n].clear();
      disp_7seg_2[n].drawColon(false);
      disp_7seg_2[n].writeDisplay();
    }
  }

  if (dtBANK4)                                 // rear display bank (place)
  {
    set_display_bank(BANK4);

    for (uint8_t n=0; n<NUM_LANES; n++)
    {
      disp_8x8m_2[n] = Adafruit_8x8matrix();
      disp_8x8m_2[n].begin(DISP_ADD[n]);
      disp_8x8m_2[n].setTextSize(1);
      disp_8x8m_2[n].setRotation(3);
      disp_8x8m_2[n].clear();
      disp_8x8m_2[n].writeDisplay();
    }
  }

  return;

#endif
}


/*================================================================================*
  UPDATE LANE PLACE/TIME DISPLAY
 *================================================================================*/
void update_display(uint8_t pos, const uint8_t msgL[], const uint8_t msgS[])
{

  if (dtBANK1 == dt7seg || dtBANK1 == dtL7sg)     // front display bank (time or place/time)
  {
    set_display_bank(BANK1);
    display_msg_7seg(&disp_7seg_1[pos], msgL, (dtBANK1 == dtL7sg));
  }

  if (dtBANK2 == dt8x8m)                          // front display bank (place)
  {
    set_display_bank(BANK2);
    display_msg_8x8m(&disp_8x8m_1[pos], msgS);
  }

  if (dtBANK3 == dt7seg || dtBANK3 == dtL7sg)     // rear display bank (time or place/time)
  {
    set_display_bank(BANK3);
    display_msg_7seg(&disp_7seg_2[pos], msgL, (dtBANK3 == dtL7sg));
  }

  if (dtBANK4 == dt8x8m)                          // rear display bank (place)
  {
    set_display_bank(BANK4);
    display_msg_8x8m(&disp_8x8m_2[pos], msgS);
  }

  return;
}


/*================================================================================*
  UPDATE LANE PLACE/TIME DISPLAY
 *================================================================================*/
void update_display(uint8_t pos, uint8_t place, unsigned long dtime, boolean mode)
{

  if (dtBANK1 == dt7seg || dtBANK1 == dtL7sg)   // front display bank (time or place/time)
  {
    set_display_bank(BANK1);
    if (mode)
      display_place_7seg(&disp_7seg_1[pos], place, (dtBANK1 == dtL7sg));
    else
      display_time_7seg(&disp_7seg_1[pos], dtime, (dtBANK1 == dtL7sg));
  }

  if (dtBANK2 == dt8x8m)                        // front display bank (place)
  {
    set_display_bank(BANK2);
    display_place_8x8m(&disp_8x8m_1[pos], place);
  }

  if (dtBANK3 == dt7seg || dtBANK3 == dtL7sg)   // rear display bank (time or place/time)
  {
    set_display_bank(BANK3);
    if (mode)
      display_place_7seg(&disp_7seg_2[pos], place, (dtBANK3 == dtL7sg));
    else
      display_time_7seg(&disp_7seg_2[pos], dtime, (dtBANK3 == dtL7sg));
  }

  if (dtBANK4 == dt8x8m)                       // rear display bank (place)
  {
    set_display_bank(BANK4);
    display_place_8x8m(&disp_8x8m_2[pos], place);
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

  if (!SHOW_PLACE || dtBANK2 || dtBANK4) return;

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


/*================================================================================*
  SELECT DISPLAY BANK VIA MULTIPLEXER
 *================================================================================*/
void set_display_bank(uint8_t b)
{
#ifndef LED_DISPLAY
  return;
#else

  if (b > 7) return;
 
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(1 << b);
  Wire.endTransmission();  

  return;
#endif
}


/*================================================================================*
  SCAN FOR AVAILABLE DISPLAYS
 *================================================================================*/
void scan_i2c_multiplexer()
{
  char tmps[25];

  smsg_str("\nScanning TCA9548A I2C multiplexer:\n");
    
  for (uint8_t bank=0; bank<8; bank++)
  {
    set_display_bank(bank);
    sprintf(tmps, "  TCA Bank #%d", bank); smsg_str(tmps);

    for (uint8_t addr=112; addr<=119; addr++)    // from 0x70 to 0x77
    {
      if (addr == I2C_ADDR) continue;

      Wire.beginTransmission(addr);
      if (!Wire.endTransmission())
      {
        sprintf(tmps, "    Found I2C 0x%x", addr); smsg_str(tmps);
      }
    }
  }
  smsg_str("\ndone\n");

  return;
}

