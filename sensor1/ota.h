#ifndef __OTA_H
#define __OTA_H

#include <ESP8266httpUpdate.h>

const int i_firmwareVersion = FW_VERSION;
const char* fwUrlBase = "http://192.168.2.5/firmware/";

void checkForUpdates(String mac);

#endif
