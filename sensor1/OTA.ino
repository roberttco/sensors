#ifdef DEBUG

void update_started() {
  Serial.println("CALLBACK:  HTTP update process started");
}

void update_finished() {
  Serial.println("CALLBACK:  HTTP update process finished");
}

void update_progress(int cur, int total) {
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}

#endif

void checkForUpdates(String mac) 
{
  String fwURL = String( fwUrlBase );
  fwURL.concat( mac );
  String fwVersionURL = fwURL;
  fwVersionURL.concat( "/version.txt" );

  DEBUG_PRINTLN( "Checking for firmware updates." );
  DEBUG_PRINT( "MAC address: " );
  DEBUG_PRINTLN( mac );
  DEBUG_PRINT( "Firmware version URL: " );
  DEBUG_PRINTLN( fwVersionURL );

  HTTPClient httpClient;
  httpClient.begin( fwVersionURL );
  int httpCode = httpClient.GET();
  if( httpCode == 200 ) 
  {
    String newFWVersion = httpClient.getString();

    DEBUG_PRINT( "Current firmware version: " );
    DEBUG_PRINTLN( i_firmwareVersion );
    DEBUG_PRINT( "Available firmware version: " );
    DEBUG_PRINTLN( newFWVersion );

    int newVersion = newFWVersion.toInt();

    if( newVersion != i_firmwareVersion )
    {
      DEBUG_PRINTLN( "Preparing to update" );

      String fwImageURL = fwURL;
      fwImageURL.concat( "/firmware.bin" );

#ifdef DEBUG
      ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
      ESPhttpUpdate.onStart(update_started);
      ESPhttpUpdate.onEnd(update_finished);
      ESPhttpUpdate.onProgress(update_progress);
      ESPhttpUpdate.onError(update_error);
#endif

      t_httpUpdate_return ret = ESPhttpUpdate.update( fwImageURL );

      if (ret == HTTP_UPDATE_OK)
      {
        DEBUG_PRINTLN("Clearing RTC data structure.");
        memset(rtcData,0,sizeof(RtcData));

        rtcData->loopsBeforeScan = 0;
        
        // after clearing the RTC memory, wait 10 seconds and reboot so there is time to
        // reconnect the sensor wire.
        delay(10000);
      }

#ifdef DEBUG
      switch(ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          break;

        case HTTP_UPDATE_OK:
          Serial.println("HTTP_UPDATE_OK");
          break;
      }
#endif
    }
    else 
    {
      DEBUG_PRINTLN( "Already on latest version" );
    }
  }
  else 
  {
    DEBUG_PRINT( "Firmware version check failed, got HTTP response code " );
    DEBUG_PRINTLN( httpCode );
  }
  httpClient.end();
}
