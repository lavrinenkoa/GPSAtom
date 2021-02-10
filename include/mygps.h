#ifndef MYGPS_h
#define MYGPS_h

//#define M5STACK
#define M5ATOM

#ifdef M5STACK
#include <M5Stack.h>
#endif
#ifdef M5ATOM
#define FASTLED_INTERNAL 1

#include "M5Atom.h"
#include "FS.h"
#include <SD.h>

CRGB HSVtoRGB(uint16_t h, uint16_t s, uint16_t v);
#endif

struct ConfigParam {
  char hostname[64];
  int port;
};

#endif
