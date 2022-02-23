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

