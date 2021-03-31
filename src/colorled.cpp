#include "mygps.h"

#ifdef M5ATOM

CRGB led(0, 0, 0);

// static bool red_st=  false;
// static bool green_st=false;
static bool blue_st= false;
// static bool white_st=false;

void red()
{
    if (blue_st) return;
    led = CRGB(0,255,0);
    M5.dis.drawpix(0, led);
}

void green()
{
    if (blue_st) return;
    led = CRGB(255,0,0);
    M5.dis.drawpix(0, led);
}

void blue()
{
    led = CRGB(0,0,255);
    M5.dis.drawpix(0, led);
}

void blue_on()
{
    blue_st = true;
    led = CRGB(0,0,255);
    M5.dis.drawpix(0, led);
}

void blue_off()
{
    blue_st = false;
    led = CRGB(0,0,0);
    M5.dis.drawpix(0, led);
}

void white()
{
    if (blue_st) return;
    led = CRGB(255,255,255);
    M5.dis.drawpix(0, led);
}
#else
// Todo: for the Fire
void red(){};
void green(){};
void blue(){};
void white(){};
void blue_on();
void blue_off();
#endif