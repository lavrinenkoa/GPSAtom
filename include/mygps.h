#ifndef MYGPS_h
#define MYGPS_h

//#define M5STACK
#define M5ATOM

#ifdef M5STACK
#include <M5Stack.h>
#endif
#ifdef M5ATOM
#define FASTLED_INTERNAL 1 // for FASTLED library
#include "M5Atom.h"
#include "FS.h"
#include <SD.h>

CRGB HSVtoRGB(uint16_t h, uint16_t s, uint16_t v);
#endif

#define CONF_STR_LEN      ( 64 )
#define CONF_WIFI_AP      (  0 ) 
#define CONF_WIFI_CLIENT  (  1 ) 

struct ConfigParam {
  int dbg;
  int sd_start_timeout;
  int wifi_mode;
  char wifi_ap_name[CONF_STR_LEN];
  char wifi_ap_password[CONF_STR_LEN];
  char wifi_server_dns[CONF_STR_LEN];

  char wifi_ssid[CONF_STR_LEN];
  char wifi_ssid_password[CONF_STR_LEN];

  char GPX_meta_desc[CONF_STR_LEN];
  char GPX_track_name[CONF_STR_LEN];
  char GPX_track_description[CONF_STR_LEN];
  char GPX_src[CONF_STR_LEN];
};

#endif
