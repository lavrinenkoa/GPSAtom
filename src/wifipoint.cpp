#include "mygps.h"

#include <DNSServer.h>
#include <WiFi.h>
#include "dbg.h"

#define FILE_BUFF_SIZE     ( 1024 )

//------------------- Wi-Fi Server --------------------
extern ConfigParam config;   // Access Point credentials

DNSServer dnsServer;
WiFiServer server(80);
WiFiClient client;
int client_wifi_status;

#define CLIENT_WIFI_CONNECTED (0)

void initWifiServer()
{
    WiFi.mode(WIFI_AP_STA);  //need both to serve the webpage and take commands via tcp
    IPAddress ip(1,2,3,4);
    IPAddress gateway(1,2,3,1);
    IPAddress subnet(255,255,255,0);
    WiFi.softAPConfig(ip, gateway, subnet);

    dbg("Setting soft-AP ... ");
    boolean result = WiFi.softAP(config.wifi_ap_name, config.wifi_ap_password);
    if (result == true) dbg("Ready\n");
    else                dbg("Failed!\n");

    IPAddress ip_address = WiFi.softAPIP();
    dbg("AP IP address: %s\n", ip_address.toString().c_str());


    const byte DNS_PORT = 53;
    dnsServer.start(DNS_PORT, config.wifi_server_dns, ip_address);
}


void initWifiClient()
{
  int n=0;

  dbg("Connect to WiFi %s\n", config.wifi_ssid)
  WiFi.begin(config.ssid, config.wifi_ssid_password);
  while (1)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      client_wifi_status = CLIENT_WIFI_CONNECTED;
      dbg("\n");
      dbg("Connection established");  
      dbg("IP address:\t");
      dbg(WiFi.localIP());   
      break;
    }

    delay(500);
    dbg(".");
    ++n;
    if (n>30){
      client_wifi_status = CLIENT_WIFI_CONNECTED_TIMEOUT;
      dbg("\n");
      dbg("Connection timeout. Wi-fi disabled.");
      break;
    }
  }

}


//---------------------- HTTP Server ---------------------------------------

void printDirectory(File dir, int numTabs) {
    while (true)
    {
        File entry = dir.openNextFile();
        if (!entry)
            break; // no more files
        for (uint8_t i = 0; i < numTabs; i++)
            dbg("\t");
        dbg(entry.name());
        if (entry.isDirectory())
        {
            dbg("/\n");
            printDirectory(entry, numTabs + 1);
        }
        else
        {
            // files have sizes, directories do not
            dbg("\t\t");
            dbg("%d %d\n", entry.size(), DEC);
        }
        entry.close();
    }
}

void initHTTPServer()
{
    File sd_root;

    server.begin(80);

    sd_root = SD.open("/");
    dbg("Files found in all dirs:\n");
    printDirectory(sd_root, 0);

    sd_root.close();
}

void listFilesHTTP(uint8_t flags, File dir)
{
    client.println("<ul>");
    while (true)
    {
        File entry = dir.openNextFile();

        // done if past last used entry
        if (!entry)
            break; // no more files

        if (strcmp(entry.name(), "/favicon.ico") == 0 or
//         strcmp(entry.name(), "/debug.txt")   == 0 or
                strcmp(entry.name(), "/config.txt") == 0)
        {
            entry.close();
            continue;
        }

        // print any indent spaces
        client.print("<a href=\"");
        client.print(entry.name());
        if (entry.isDirectory())
            client.println("/");
        client.print("\">");

        // print file name with possible blank fill
        client.print(entry.name() + 1);
        if (entry.isDirectory())
            client.println("/");

        client.print("</a>");

//    // print modify date/time if requested
//    if (flags & LS_DATE) {
//       dir.printFatDate(p.lastWriteDate);
//       client.print(' ');
//       dir.printFatTime(p.lastWriteTime);
//    }

//    // print size if requested
//    if (!DIR_IS_SUBDIR(&p) && (flags & LS_SIZE))
        {
            size_t size = entry.size();
            client.print("&nbsp;&nbsp;&nbsp;&nbsp;[ ");
            if (size >= 1024)
            {
                client.print(size / 1024, DEC);
                client.println(" Kb ]");
            }
            else
            {
                client.print(size , DEC);
                client.println(" byte ]");
            }
            client.println("<br>");
        }
    entry.close();
  }
  client.println("</ul>");
}

void loopHTTP()
{
    dnsServer.processNextRequest();

    char clientline[BUFSIZ];
//    char name[17];
    int index = 0;

    client = server.available();
    if (client) {
      // an http request ends with a blank line
//      boolean current_line_is_blank = true;

      // reset the input buffer
      index = 0;

      while (client.connected()) {
        if (client.available()) {
          char c = client.read();

          // If it isn't a new line, add the character to the buffer
          if (c != '\n' && c != '\r') {
            clientline[index] = c;
            index++;
            // are we too big for the buffer? start tossing out data
            if (index >= BUFSIZ)
              index = BUFSIZ -1;

            // continue to read more data!
            continue;
          }

          // got a \n or \r new line, which means the string is done
          clientline[index] = 0;

          // Print it out for debugging
          dbg("%s\n", clientline);

          // Look for substring such as a request to get the file
          if (strstr(clientline, "GET /") != 0)
          {
            // this time no space after the /, so a sub-file!
//            char *filename;
            String filename;
            char *filename_str;

            filename_str = clientline + 5; // look after the "GET /" (5 chars)  *******
            // a little trick, look for the " HTTP/1.1" string and
            // turn the first character of the substring into a 0 to clear it out.
            (strstr(clientline, " HTTP"))[0] = 0;

            if(filename[strlen(filename_str)-1] == '/') {  // Trim a directory filename
              filename[strlen(filename_str)-1] = 0;        //  as Open throws error with trailing /
            }

            dbg("Web request for: >%s<\n", filename_str);
            filename = "/" + String(filename_str);

            File file = SD.open(filename.c_str(), "r");
            if ( file == 0 ) {  // Opening the file with return code of 0 is an error in SDFile.open
              client.println("HTTP/1.1 404 Not Found");
              client.println("Content-Type: text/html");
              client.println();
              client.println("<h2>File Not Found!</h2>");
              client.println("<br><h3>Couldn't open the File!</h3>");
              break;
            }

            dbg("File Opened!\n");

            client.println("HTTP/1.1 200 OK");
            if (file.isDirectory()) {
              Serial.println("is a directory");
              //file.close();
              client.println("Content-Type: text/html");
              client.println();
              client.print("<font size=\"6\">");
              client.print("<h2>LVR GPS track files");
//              client.print(filename); dir name
              client.println(":</h2>");
              //ListFiles(client,LS_SIZE,file);
              listFilesHTTP(0,file);
              file.close();
            } else { // Any non-directory clicked, server will send file to client for download
              client.println("Content-Type: application/octet-stream");
              client.println();

              char file_buffer[FILE_BUFF_SIZE];
              int avail;
              while (1) {
                  avail = file.available();
                  if (!avail)
                      break;
                  int to_read = min(avail, FILE_BUFF_SIZE);
                  if (to_read != file.read((uint8_t*)file_buffer, to_read)) {
                  break;
                }
                // uncomment the serial to debug (slow!)
                //Serial.write((char)c);
                client.write(file_buffer, to_read);
              }
              file.close();
            }
          } else {
            // everything else is a 404
            client.println("HTTP/1.1 404 Not Found");
            client.println("Content-Type: text/html");
            client.println();
            client.println("<h2>File Not Found!</h2>");
          }
          break;
        }
      }
      // give the web browser time to receive the data
      delay(1);
      client.stop();
    }
}

