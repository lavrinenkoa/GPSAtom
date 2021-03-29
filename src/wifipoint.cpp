#include "mygps.h"
#include <DNSServer.h>
#include <WiFi.h>
#include <ESP_Mail_Client.h>

#include "dbg.h"
#include "wifipoint.h"

#define FILE_BUFF_SIZE     ( 1024 )

extern char gpxTrackName[20];
//------------------- Email sender --------------------
SMTPSession smtp;
#include "passwd.h"
void smtpCallback(SMTP_Status status);  //  Callback function to get the Email sending status


//------------------- Wi-Fi Server --------------------
extern ConfigParam config;   // Access Point credentials

DNSServer dnsServer;
WiFiServer server(80);
WiFiClient client;
int client_wifi_status;

#define CLIENT_WIFI_CONNECTED         ( 0 )
#define CLIENT_WIFI_CONNECTED_TIMEOUT ( 1 )

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

  dbg("Connect to WiFi %s\n", config.wifi_ssid);
  WiFi.begin(config.wifi_ssid, config.wifi_ssid_password);
  while (1)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      client_wifi_status = CLIENT_WIFI_CONNECTED;
      dbg("\n");
      dbg("Connection established\n");  
      dbg("IP address:\t %s\n", WiFi.localIP().toString().c_str());
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

void disconnectWifiClient()
{
  WiFi.disconnect();
}

// https://github.com/mobizt/ESP-Mail-Client/blob/master/examples/Send_Attachment_File/Send_Attachment_File.ino
int sendEmail(String file_attachment)
{
  int ret=-1;

  // ESP_Mail_Client mclient;
  // mclient.sdBegin();
  int sdret = MailClient.sdBegin();
  dbg("Email SD init %d\n", sdret);

  // Enable the debug via Serial port
  // none debug or 0
  // basic debug or 1
  smtp.debug(1);
  smtp.callback(smtpCallback);   // Set the callback function to get the sending results
  ESP_Mail_Session session;      // Declare the session config data

  // Set the session config
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "mydomain.net";
  session.secure.startTLS = true;
  // session.secure.cert_file;              ToDo
  // The OAuth2.0 access token to log in
  // session.login.accessToken = "";
  
  SMTP_Message message;

  // Set the message headers
  char messageText[40] = {0};
  char subjectText[40] = {0};
  sprintf(subjectText, "GPS Track %s", file_attachment.c_str()+1);
  sprintf(messageText, "GPS Track %s attached.", file_attachment.c_str()+1);
  message.sender.name =  "ESP GPSAtom";
  message.sender.email = AUTHOR_EMAIL;
  message.subject =      subjectText;
  message.addRecipient("GPSAtom", "gpsm5stack@gmail.com");
  message.text.content = "This is simple plain text message";
  message.text.content = messageText;

  SMTP_Attachment att;
  att.descr.filename = file_attachment.c_str()+1;
  att.descr.mime = file_attachment.c_str()+1;
  att.file.path = file_attachment.c_str();
  att.file.storage_type = esp_mail_file_storage_type_sd; // from SD
  att.descr.transfer_encoding = Content_Transfer_Encoding::enc_base64;
  message.addInlineImage(att);

  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_high;
  message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;

  // Connect to server with the session config
  // if (!smtp.connect(&session))
  // {
  //   dbg("Atom: Can not connect to the server: %s\n", smtp.errorReason().c_str());
  //   smtp.closeSession();
  //   return -1;
  // }
  for (int i=0;i<=3;i++)
  {
      ret = smtp.connect(&session);
      if (ret == 1) break;
      if ( (ret==0) and (i<=2)){
          dbg("Atom: Can not connect to the server: %s\n", smtp.errorReason().c_str());
          dbg("waitig 3 seconds...");
          sleep(3);
          dbg("and try again!\n");
      }
  }

  if (ret)
  {
    dbg("Atom: SMTP Connected OK\n");
    }
  else
  {
    dbg("Atom:  SMTP Connect Error: %s.\n", smtp.errorReason().c_str());
    ret = -1;
    return ret;
  }

  

  // Start sending Email and close the session
  // ret = MailClient.sendMail(&smtp, &message);
  for (int i=0;i<=3;i++)
  {
      ret = MailClient.sendMail(&smtp, &message);
      if (ret) break;
      if ( (ret!=true) and (i<=2)){
          dbg("Atom: Email send is failed :( Let's try again!\n");
          sleep(3);
      }
  }
  if (ret)
  {
    dbg("Atom: Email sent successfully\n");
    ret = 0;
  }
  else
  {
    dbg("Atom: Error sending Email, %s.\n", smtp.errorReason().c_str());
    ret = -1;
  }

  smtp.closeSession();

  return ret;
}

// Callback function to get the Email sending status
void smtpCallback(SMTP_Status status)
{
  // Print the current status
  Serial.println(status.info());

  // Print the sending result
  if (status.success())
  {
    Serial.println("----------------");
    Serial.printf("Message sent success: %d\n", status.completedCount());
    Serial.printf("Message sent failled: %d\n", status.failedCount());
    Serial.println("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
      // Get the result item
      SMTP_Result result = smtp.sendingResult.getItem(i);
      localtime_r(&result.timesstamp, &dt);

      Serial.printf("Message No: %d\n", i + 1);
      Serial.printf("Status: %s\n", result.completed ? "success" : "failed");
      Serial.printf("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      Serial.printf("Recipient: %s\n", result.recipients);
      Serial.printf("Subject: %s\n", result.subject);
    }
    Serial.println("----------------\n");
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

