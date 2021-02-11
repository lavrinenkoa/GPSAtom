#include "mygps.h"

#define DBG_FILE_TXT       "/debug.txt"
#define LCD_MAX_LOG_LINE   ( 20 )

#ifdef M5STACK
int lcdLine=0;
#endif

extern ConfigParam config;   

void dbg(const char* format, ...)
{
    if (config.dbg == 0) return;

    File logFile;
    char tmp[1024*2];
    va_list argptr;
    va_start(argptr, format);
    vsprintf(tmp, format, argptr);
    va_end(argptr);

#ifdef M5STACK
    if ( lcdLine%LCD_MAX_LOG_LINE == 0 )
    {
        M5.Lcd.clear();
        M5.Lcd.setCursor(0, 0);
    }
    lcdLine++;

    M5.Lcd.printf(tmp);
#endif
    Serial.printf(tmp);
    logFile = SD.open(DBG_FILE_TXT, "a+");
    logFile.printf(tmp);
    logFile.flush();
    logFile.close();
}

void dbg_serial(const char* format, ...)
{
    if (config.dbg == 0) return;

    char tmp[1024*2];
    va_list argptr;
    va_start(argptr, format);
    vsprintf(tmp, format, argptr);
    va_end(argptr);
    Serial.printf(tmp);

#ifdef M5STACK
    if ( lcdLine%LCD_MAX_LOG_LINE == 0 )
    {
        M5.Lcd.clear();
        M5.Lcd.setCursor(0, 0);
    }
    lcdLine++;
#endif
}


