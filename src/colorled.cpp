#include "mygps.h"

#ifdef GPSATOM

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

void blue_off(bool st=false)
{
    blue_st = st;
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
// #include <Adafruit_NeoPixel.h>
// #define M5STACK_FIRE_NEO_NUM_LEDS 10
// #define M5STACK_FIRE_NEO_DATA_PIN 15
// extern Adafruit_NeoPixel pixels;

extern SemaphoreHandle_t LCDSemaphore;

static bool blue_st= false;
bool skipp = false;

void lsdLED(uint32_t color)
{
    // return;
    if ( xSemaphoreTake( LCDSemaphore, ( TickType_t ) 15 ) == pdTRUE ){
        M5.Lcd.fillRect(0, M5.Lcd.height()-20, M5.Lcd.width(), 20, color);
        xSemaphoreGive(LCDSemaphore);
    }
}

void red()
{
    if (skipp) return;
    if (blue_st) return;
    lsdLED(RED);
}

void green()
{
    if (skipp) return;
    if (blue_st) return;
    lsdLED(GREEN);
}
void blue()
{
    if (skipp) return;
    blue_st = true;
}
void white()
{
    if (skipp) return;
    if (blue_st) return;
    lsdLED(WHITE);
}
void blue_on()
{
    if (skipp) return;
    blue_st = true;
    lsdLED(BLUE);
}
void blue_off(bool st=false)
{
    if (skipp) return;
    blue_st = st;
    lsdLED(BLACK);
}
#endif