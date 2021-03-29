#include "mygps.h"

#ifdef M5ATOM

CRGB led(0, 0, 0);

// static bool red_st=  false;
// static bool green_st=false;
// static bool blue_st= false;
// static bool white_st=false;

void red()
{
    
    led = CRGB(0,255,0);
    M5.dis.drawpix(0, led);
}

void green()
{
    led = CRGB(255,0,0);
    M5.dis.drawpix(0, led);
}

void blue()
{
    led = CRGB(0,0,255);
    M5.dis.drawpix(0, led);
}

void white()
{
    led = CRGB(255,255,255);
    M5.dis.drawpix(0, led);
}
#else
// Todo: for the Fire
void red(){};
void green(){};
void blue(){};
void white(){};
#endif