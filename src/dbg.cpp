#include "mygps.h"

#define DBG_FILE_TXT       "/debug.txt"
#define LCD_MAX_LOG_LINE   ( 20 )

#ifdef M5STACK
int lcdLine=0;
#endif

extern ConfigParam config;   
extern SemaphoreHandle_t SDSemaphore;
extern SemaphoreHandle_t LCDSemaphore;

void dbg(const char* format, ...)
{
    // if (config.dbg == 0) return;
    File logFile;
    char tmp[1024*2];
    va_list argptr;
    va_start(argptr, format);
    vsprintf(tmp, format, argptr);
    va_end(argptr);

#ifdef GPSFIRE
    // xSemaphoreTake(LCDSemaphore, 0);
    if ( xSemaphoreTake( LCDSemaphore, ( TickType_t ) 15 ) == pdTRUE )
    {
        if ( lcdLine%LCD_MAX_LOG_LINE == 0 )
        {
            // M5.Lcd.clear();
            M5.Lcd.fillRect(0, 0, M5.Lcd.width(), M5.Lcd.height()-20, BLACK);
            M5.Lcd.setCursor(0, 0);
        }
        lcdLine++;
        M5.Lcd.printf(tmp);

        xSemaphoreGive(LCDSemaphore);
    }
#endif

    Serial.printf(tmp);

    if (config.dbg)
    if ( xSemaphoreTake( SDSemaphore, ( TickType_t ) 15 ) == pdTRUE )
    {
        logFile = SD.open(DBG_FILE_TXT, "a+");
        logFile.printf(tmp);
        logFile.flush();
        logFile.close();
        xSemaphoreGive(SDSemaphore);
    }

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
}


