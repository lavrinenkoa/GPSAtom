#ifndef WIFIPOINT_H_
#define WIFIPOINT_H_

#define CLIENT_WIFI_CONNECTED         ( 0 )
#define CLIENT_WIFI_CONNECTED_TIMEOUT ( 1 )
#define CLIENT_WIFI_DISCONNECTED      ( 2 )

void initWifiServer();
int initWifiClient(String ssid, String password);
void initHTTPServer();
void disconnectWifiClient();
void loopHTTP();
int sendEmail(String file_attachment);

#endif /* WIFIPOINT_H_ */
