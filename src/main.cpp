/****************************************************************************************/
/* Car GPS Logger with M5Stack and M5 GPS Module                                        */
/* Author: Alexandr Lavrinenko                                                          */
/* Version: 0.1                                                                         */
/*      Used library:                                                                   */
/*                        Tiny GPS Plus: https://github.com/mikalhart/TinyGPSPlus       */
/*                        ArduinoJson:   https://github.com/bblanchon/ArduinoJson       */
/*                        BLE                                                           */
/*                        BluetoothSerial                                               */
/*                        DNSServer                                                     */
/*                        FS                                                            */
/*                        HTTPClient                                                    */
/*                        M5Stack                                                       */
/*                        SD                                                            */
/*                        SPI                                                           */
/*                        SPIFFS                                                        */
/*                        WiFi                                                          */
/*                        WiFiClientSecure                                              */
/*                        Wire                                                          */
/****************************************************************************************/
#include <TinyGPS++.h>
#include <ArduinoJson.h>
#include <map>
#include <string>

#include "dbg.h"
#include "colorled.h"
#include "mygps.h"
#include "wifipoint.h"
#include "GPX.h"

#define CFG_FILE_TXT       "/config.txt"
#define GPS_STAUS_NO_SAT   ( 0  )  // No GPS receiver detected
#define GPS_STAUS_NO_VALID ( 1  )  // No valid GPS fix
#define GPS_STAUS_VALID    ( 2  )  // Valid GPS data
#define DATA_TIME_STR_LEN  ( 44 )
#define LCD_BRIGHTNESS     ( 250 )

#define GPS_VALUE_VALID              ( 0b00000000 )
#define GPS_VALUE_NOT_VALID_HDOP     ( 0b00000001 )
#define GPS_VALUE_NOT_VALID_DATE     ( 0b00000100 )
#define GPS_VALUE_NOT_VALID_SAT      ( 0b00001000 )
#define GPS_VALUE_NOT_VALID_LOCATION ( 0b00010000 )
#define GPS_VALUE_FILTERED           ( 0b00100000 )

const char *cfgFileName = CFG_FILE_TXT;
ConfigParam config;                                           // <- global configuration object

TinyGPSPlus gps;                                              // Reference the TinyGPS++ object
HardwareSerial GPSRaw(2);                                     // By default, GPS is connected with M5Core through UART2

// GPX track file
GPX gpxTrack;
char gpxTrackName[20];
char gpxTrackFileName[20];
int gpxTrackFileStartPause = 5; // ~ sec  pause before gpx file started
boolean gpx_file_started=false;

TaskHandle_t Task0GPS;
TaskHandle_t Task1HTTP;
TaskHandle_t Task1WifiClient;

void loadConfiguration(const char *filename, ConfigParam &config);
void TaskGPS( void * pvParameters );
void TaskWifiClient( void * pvParameters );
void TaskHTTP( void * pvParameters );
void gpsProcessing();
void gpsLogValues(); 
int isGPSValuesFiltered();
int isGPSValuesValid();
void GPXFileUpdate();
void GPXFileInit();
void GPXFileSaveTrack(String gpx_point);

/****************************************************************************************/
/* Init routine                                                                         */
/****************************************************************************************/


int SDCardInit()
{
#ifdef M5STACK
    // Fire
    // SD file create
    dbg("Initializing SD card...");
    if (!SD.begin()) {
      dbg("FAILED!\n");
      return -1;
    }
    dbg("OK\n");
#endif

#ifdef M5ATOM
    dbg("Initializing SD card...");
    SPI.begin(23,33,19,-1);
    if(!SD.begin(-1, SPI, 40000000)){
      dbg("initialization failed!");
      return -1;
    } 
    sdcard_type_t Type = SD.cardType();

	dbg("SDCard Type = %d \r\n",Type);
	dbg("SDCard Size = %d \r\n" , (int)(SD.cardSize()/1024/1024));

    dbg("OK\n");
    FastLED.setBrightness(1);
    white();
#endif

    return 0;
}

int syncMailAndSDFiles()
{
    int ret=-1;
    int i=0;
    std::map <String, int> sd_tracks;
    std::map <String, int> csv_tracks;
    std::map <String, int> to_mail_tracks;
    std::map <String, int> :: iterator it = sd_tracks.begin();
    std::map <String, int> :: iterator sd;
    std::map <String, int> :: iterator csv;

    File csv_file;
    String file_name;
    String file_size;

    // Get SD *.gpx file list.
    File dir = SD.open("/");
    dbg("-------------------------\n");
    dbg("SD file list:\n");
    while (true)
    {
        File entry = dir.openNextFile();
        if (!entry)
            break; // no more files
        if (strstr(entry.name(), ".gpx") == NULL)
        {
            entry.close();
            continue;
        }
        dbg("%d %s %d\n", ++i, entry.name(), entry.size());
        // if (entry.isDirectory())
        sd_tracks[entry.name()] = entry.size();
        entry.close();
    }
    dbg("-------------------------\n");

    // Read  CSV file with previus sync results
    csv_file  = SD.open("/sync.csv", FILE_READ);
    // if (!tracks_file.isOpen())
    // {
    //     dbg("Error: can not open file sync.csv\n");
    //     return -1;
    // }
    dbg("-------------------------\n");
    dbg("CSV file list:\n"); i=0;
    while (csv_file.available()) {
        file_name = csv_file.readStringUntil(',');
        file_size = csv_file.readStringUntil('\n');
        dbg("%d %s %d\n", ++i, file_name.c_str(), file_size.toInt());
        csv_tracks[file_name] = file_size.toInt();
    }
    csv_file.close();
    dbg("-------------------------\n");

    // Check unsynchronized files and send emails
    String sd_file;
    int    sd_size;
    it = sd_tracks.begin();
    for (int i = 0; it != sd_tracks.end(); it++, i++) {
        dbg("%d %s %d\n", i, it->first.c_str(), it->second);
        sd_file = it->first;
        sd_size = it->second;

        csv = csv_tracks.find(sd_file);
        if (csv == csv_tracks.end())   // file not found in csv list, send email
        {
            to_mail_tracks[sd_file] = sd_size;
            dbg("Send email with %s %d\n", sd_file.c_str(), sd_size);
            ret = sendEmail(sd_file);
            if (ret==0) csv_tracks[sd_file] = sd_size;
        }
        else
        {
            if (sd_size != csv->second) // file size updated, send email
            {
                to_mail_tracks[sd_file] = sd_size;
                dbg("Send email with %s %d. size updated\n", sd_file.c_str(), sd_size);
                ret = sendEmail(sd_file);
                if (ret==0) csv_tracks[sd_file] = sd_size;
            }
            else
            {
                // add to csv
                csv_tracks[sd_file] = sd_size;
                dbg("Add file %s %d\n", sd_file.c_str(), sd_size);
            }
        }
    }

    // Update CSV file
    dbg("Update sync.csv\n");
    csv_file  = SD.open("/sync.csv", FILE_WRITE);
    it = csv_tracks.begin();
    for (int i = 0; it != csv_tracks.end(); it++, i++) {
        dbg("%d %s %d\n", i, it->first.c_str(), it->second);
        // file_name, size
        csv_file.printf("%s, %d\n", it->first.c_str(), it->second);
    }
    csv_file.close();

    return ret;
}

void setup()
{
#ifdef M5STACK
    M5.begin();                                    // Start M5 functions
    GPSRaw.begin(9600);                            // Init GPS serial interface
    //M5.Lcd.setBrightness(LCD_BRIGHTNESS - 200);    // Set initial LCD brightness
#endif

#ifdef M5ATOM
    M5.begin(true,false,true); 
    GPSRaw.begin(9600,SERIAL_8N1,22,-1);           // Init GPS serial interface 
#endif

    delay(2000);                                   // 2000ms init delay
    gpx_file_started = false;

    dbg("Running...\n");

    if (SDCardInit())
        return;

    loadConfiguration(cfgFileName, config);

    if ( config.wifi_mode == CONF_WIFI_AP )
    {
        initWifiServer();
        initHTTPServer();
    }

    xTaskCreatePinnedToCore(
                      TaskGPS,     // Task function.
                      "GPS",       // name of task.
                      50*1024,     // Stack size of task
                      NULL,        // parameter of the task
                      1,           // priority of the task
                      &Task0GPS,   // Task handle to keep track of created task
                      0);          // pin task to core 0

    delay(100);

    if ( config.wifi_mode == CONF_WIFI_AP )
    {
        xTaskCreatePinnedToCore(
                        TaskHTTP,    // Task function.
                        "HTTP",      // name of task.
                        50*1024,     // Stack size of task
                        NULL,        // parameter of the task
                        1,           // priority of the task
                        &Task1HTTP,  // Task handle to keep track of created task
                        1);          // pin task to core 1
        delay(100);
    }
    else if ( config.wifi_mode == CONF_WIFI_CLIENT )
    {
        xTaskCreatePinnedToCore(
                        TaskWifiClient,    // Task function.
                        "Wifi",      // name of task.
                        5*1024,     // Stack size of task
                        NULL,        // parameter of the task
                        1,           // priority of the task
                        &Task1WifiClient,  // Task handle to keep track of created task
                        1);          // pin task to core 1
        delay(100);
    }

      disableCore0WDT();
}

void TaskGPS( void * pvParameters )
{
    while(1) sleep(5);
    delay(2000);
    while(1)
    {
        // Serial.print("Task0GPS running on core ");
        // Serial.println(xPortGetCoreID());
        gpsProcessing();
        yield();

        // Smart delay
        int ms = 1000;
        int read_loop=0;
        unsigned long start = millis();
        do
        {
            while (GPSRaw.available() and read_loop <=10000)
            {
                gps.encode(GPSRaw.read());  // GPS read()
                read_loop++;
                // Add botton req
                yield();
            }
            yield();
        } while (millis() - start < ms);
    }
}

void TaskHTTP( void * pvParameters )
{
    while(1)
    {
        // Serial.print("Task2 running on core ");
        // Serial.println(xPortGetCoreID());
        yield();
        loopHTTP();
    }
}

void TaskWifiClient( void * pvParameters )
{
    int ret=-1;
    // while(1) sleep(5);

    initWifiClient();
    while(1)
    {
        // Serial.print("Task2 running on core ");
        // Serial.println(xPortGetCoreID());
        yield();
        //loopHTTP();
        if (M5.Btn.wasPressed())
        {
            blue();
            ret = syncMailAndSDFiles();
            if (ret == 0)
                white();
            else
                red();
        }
        delay(50);
        M5.Btn.read();
    }
}

/****************************************************************************************/
/* Main routine                                                                         */
/****************************************************************************************/
void loop()
{
    // Serial.print("Loop running on core ");
    // Serial.println(xPortGetCoreID());
    // delay(1000);
    vTaskDelay(1);
//    vTaskDelay(portMAX_DELAY);
//    vTaskDelete(NULL);
    return;

// OK
//    char c;
//    bool no_message=true;
//    int nb_messages =0;
//    do
//    {
//        nb_messages = GPSRaw.available();
//        if (nb_messages > 0)
//        {
//            c = GPSRaw.read();
////            dbg("%c", c);
//            if (gps.encode(c))
//            {
////                dbg("Processing gps messge\n");
//                gpsProcessing();
//            } // else waiting raw
//        } // else no message
//        else
//        {
//            dbg("no message.");
//            no_message = true;
//            break;
//        }
//    }
//    while (no_message);

//    OK
//    while (GPSRaw.available() > 0)    //Return the number of messages stored in a queue.
//      if (gps.encode(GPSRaw.read()))
//      {
//          gpsProcessing();
//      }
//    loopHTTP();
//    if (millis() > 5000 && gps.charsProcessed() < 10)
//    {
//      dbg("No GPS detected: check wiring.\n");
//    }
//    Serial.print(":\n");
//    delay(1000);
}


void loadConfiguration(const char *filename, ConfigParam &config)
{
    dbg("Loading configuration...\n");
    File file = SD.open(filename);
    if (!file){
        dbg("Can not open configuration file %s\n", filename);
        return;//todo return error/ok
    }

    StaticJsonDocument<512> doc;   // Allocate a temporary JsonDocument

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, file);
    if (error)
        dbg("Failed to read file, using default configuration");

    config.dbg =              doc["dbg"]              | 1;
    config.sd_start_timeout = doc["sd_start_timeout"] | 5;
    config.wifi_mode =        doc["wifi_mode"] | CONF_WIFI_CLIENT;
    strlcpy(config.wifi_ap_name,         doc["wifi_ap_name"]     | "GPS",       sizeof(config.wifi_ap_name));
    strlcpy(config.wifi_ap_password,     doc["wifi_ap_password"] | "123456789", sizeof(config.wifi_ap_password));
    strlcpy(config.wifi_server_dns,      doc["wifi_server_dns"]  | "gps.com",   sizeof(config.wifi_server_dns));

    strlcpy(config.wifi_ssid,            doc["wifi_ssid"]        | "TLC",       sizeof(config.wifi_ssid));
    strlcpy(config.wifi_ssid_password, doc["wifi_ssid_password"] | "123456789", sizeof(config.wifi_ssid_password));

    strlcpy(config.GPX_meta_desc,        doc["GPX_meta_desc"]    | "LVR Auto track", sizeof(config.GPX_meta_desc));
    strlcpy(config.GPX_src,              doc["GPX_src"]          | "GPSFire",        sizeof(config.GPX_src));
    strlcpy(config.GPX_track_name,       doc["GPX_track_name"]   | "TLC",            sizeof(config.GPX_track_name));
    strlcpy(config.GPX_track_description,doc["GPX_track_description"] | "Auto track",sizeof(config.GPX_track_description));

    file.close();

    dbg("dbg = %d\n",              config.dbg);
    dbg("sd_start_timeout = %d\n", config.sd_start_timeout);
    dbg("wifi_ap_name = %s\n",     config.wifi_ap_name);
    dbg("wifi_ap_password = %s\n", config.wifi_ap_password);
    dbg("wifi_server_dns = %s\n",  config.wifi_server_dns);

    dbg("wifi_ssid = %s\n",          config.wifi_ssid);
    dbg("wifi_ssid_password = %s\n", config.wifi_ssid_password);

    dbg("GPX_meta_desc = %s\n",    config.GPX_meta_desc);
    dbg("GPX_src = %s\n",          config.GPX_src);
    dbg("GPX_track_name = %s\n",   config.GPX_track_name);
    dbg("GPX_track_description = %s\n", config.GPX_track_description);

    gpxTrackFileStartPause = config.sd_start_timeout;
}

// Create falidated GPS string
void gpsProcessing()
{
    gpsLogValues();
    if ( isGPSValuesFiltered() == GPS_VALUE_FILTERED)
        return;

    GPXFileUpdate();
}

void gpsLogValues()
{
// GPX parameters
//    gps.date.isValid();
//    gps.date.year();
//    gps.date.month();
//    gps.date.day();
//    gps.time.hour();
//    gps.time.minute();
//    gps.time.second();
//    gps.satellites.isValid();
//    gps.satellites.value();
//    gps.hdop.isValid();
//    gps.hdop.hdop();
//    gps.location.isValid();
//    gps.location.lat();
//    gps.location.lng();
//    gps.altitude.isValid();
//    gps.altitude.meters();
//    gps.speed.isValid();
//    gps.speed.kmph();

    char time_str[DATA_TIME_STR_LEN];
    sprintf(time_str, "%4d-%02d-%02dT%02d:%02d:%02dZ",
            gps.date.year(), gps.date.month(),  gps.date.day(),
            gps.time.hour(), gps.time.minute(), gps.time.second());

    dbg("%s sat %d hdop %4.6f loc %4.6f %4.6f %d alt %5.1f spd %3.1f ",
            time_str,
            gps.satellites.value(),
            gps.hdop.hdop(),
            gps.location.lat(),
            gps.location.lng(),
            gps.location.rawLat().deg,
            gps.altitude.meters(),
            gps.speed.kmph());

    if (gps.date.isValid() and gps.date.year() >= 2020 and
        gps.hdop.isValid() and gps.hdop.hdop() < 20 and
        gps.satellites.isValid() and gps.satellites.value() >3 and
        gps.location.isValid() and gps.location.rawLat().deg!=0 and gps.location.rawLat().billionths!=0)
    {
        dbg("VALID\n");
    }
    else
    {
        dbg("NOT VALID: ");
        if ( !gps.satellites.isValid() or gps.satellites.value() <= 3) dbg("satellites ");
        if ( !gps.hdop.isValid() or gps.hdop.hdop() >= 20)   dbg("hdop ");
        if ( !gps.date.isValid() or gps.date.year() < 2020)  dbg("date ");
        if ( !gps.location.isValid() or gps.location.rawLat().deg==0 or gps.location.rawLat().billionths==0) dbg("location");
        dbg("\n");
    }
}

int isGPSValuesFiltered()
{
    int valid = GPS_VALUE_VALID;
    int filtered = GPS_VALUE_VALID;

    valid = isGPSValuesValid();
    if (valid != GPS_VALUE_VALID)
        return valid;

    // simple filter
    static double prev_lat;
    static double prev_lng;
    double d_max = 100; // spd<50km/h
    double d_min = 5;

    double delta = TinyGPSPlus::distanceBetween(
      gps.location.lat(),
      gps.location.lng(),
      prev_lat,
      prev_lng);

    if ((delta < d_min) or (delta > d_max))
    {
        // cur_lat = prev_lat;
        // cur_lng = prev_lng;
        prev_lat = gps.location.lat();
        prev_lng = gps.location.lng();
        dbg("GPS date filtered!\n");
        return GPS_VALUE_FILTERED;
    }

    // todo Removing "star"
    // todo Analyze speed, accelerometer
    // todo Kalman filter


    return filtered;
}


int isGPSValuesValid()
{
    int valid = GPS_VALUE_VALID;
    if ( !gps.satellites.isValid() or gps.satellites.value()<=3 ) valid |= GPS_VALUE_NOT_VALID_SAT;
    if ( !gps.hdop.isValid()       or gps.hdop.hdop() >= 20)      valid |= GPS_VALUE_NOT_VALID_HDOP;
    if ( !gps.date.isValid()       or gps.date.year() < 2020)     valid |= GPS_VALUE_NOT_VALID_DATE;
    if ( !gps.location.isValid()   or gps.location.rawLat().deg==0 or gps.location.rawLat().billionths==0) valid |= GPS_VALUE_NOT_VALID_LOCATION;
    
    if (valid == GPS_VALUE_VALID)
        green();
    else
        red();
  
    return valid;
}

void GPXFileUpdate()
{
    String gpx_point;
    char date_time[DATA_TIME_STR_LEN];

    if (gpx_file_started==false)
    {
        GPXFileInit();
    }

    if (isGPSValuesValid() == GPS_VALUE_VALID)
    {
        sprintf(date_time,"%4d-%02d-%02dT%02d:%02d:%02dZ", gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second());
        gpx_point = gpxTrack.getTrackPt(gps.location.lat(), gps.location.lng(), gps.altitude.meters(), gps.speed.kmph(), date_time);
        dbg_serial(gpx_point.c_str());
        GPXFileSaveTrack(gpx_point);
    }
}

void GPXFileInit()
{
    File gpxTrackFile;
    String gpx_head;
    String gpx_1st_trkpt;
    char date_time[22];

    if (gps.date.year()<=2019)
    {
        dbg("GPS date not ready. Waiting correct date...\n");
        return;
    }

    if (gpxTrackFileStartPause != 0){
        dbg("Waiting for stability %d\n", gpxTrackFileStartPause);
        gpxTrackFileStartPause--;
        return;
    }

    sprintf(gpxTrackName,     "%02d-%02d-%02d",      gps.date.year(), gps.date.month(), gps.date.day());
    sprintf(gpxTrackFileName, "/%02d-%02d-%02d.gpx", gps.date.year(), gps.date.month(), gps.date.day());

    gpxTrack.setMetaDesc(config.GPX_meta_desc);
    gpxTrack.setName(gpxTrackName);                    // or config.GPX_track_name?
    gpxTrack.setDesc(config.GPX_track_description);
    gpxTrack.setSrc(config.GPX_src);

    gpx_head = gpxTrack.getOpen() +
               gpxTrack.getMetaData() +
               gpxTrack.getTrakOpen() +
               gpxTrack.getInfo() +
               gpxTrack.getTrakSegOpen();
    Serial.print(gpx_head);
 
    if (SD.exists(gpxTrackFileName) == false)
    {
        gpxTrackFile = SD.open(gpxTrackFileName, "w+");
        gpxTrackFile.print(gpx_head);
        if (isGPSValuesValid()==GPS_VALUE_VALID)
        {
            sprintf(date_time,"%4d-%02d-%02dT%02d:%02d:%02dZ", gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second());
            gpx_1st_trkpt = gpxTrack.getTrackPt(gps.location.lat(), gps.location.lng(), gps.altitude.meters(), gps.speed.kmph(), date_time);
            gpxTrackFile.print(gpx_1st_trkpt);     // Add 1st track point
        }

        gpxTrackFile.print(GPX_TRACK_TAIL);
        gpxTrackFile.flush();
        gpxTrackFile.close();
        dbg("GPX Head saved OK\n");
    }
    else
    {
        //Check gpx size
        gpxTrackFile = SD.open(gpxTrackFileName, "r");
        unsigned long filesize = gpxTrackFile.size();
        gpxTrackFile.close();

        dbg("GPX file size %d\n", filesize);
        if (filesize == 0){
            SD.remove(gpxTrackFileName);
            dbg("GPX file is zero size !!!");
            return;
        }

        if (filesize < gpx_head.length()){
            SD.remove(gpxTrackFileName);
            dbg("the GPX file is not in the correct format !!!");
            return;
        }
    }

    dbg("GPX file started...\n");
    gpx_file_started = true;
}

void GPXFileSaveTrack(String gpx_point)
{
    if (gpx_file_started){
        File gpxTrackFile;
        gpxTrackFile = SD.open(gpxTrackFileName, "w+");
        unsigned long filesize = gpxTrackFile.size();
        filesize -= strlen(GPX_TRACK_TAIL);      // -27
        gpxTrackFile.seek(filesize);
        gpxTrackFile.print(gpx_point + GPX_TRACK_TAIL);
        gpxTrackFile.flush();
        gpxTrackFile.close();
    }
}
