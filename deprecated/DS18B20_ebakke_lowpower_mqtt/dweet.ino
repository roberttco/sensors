#ifdef __DWEET_H  // only include this code if publishing with dweet

boolean DWEET_ConnectAndSend(const char *buf)
{
  boolean rval = false;
  if (WiFi.status() == WL_CONNECTED)
  {
    DEBUG_PRINTLN("Sending to dweet.io");
    DEBUG_PRINTLN(buf);
    
    HTTPClient http;    //Declare object of class HTTPClient

    http.begin(DWEET_URL);      //Specify request destination
    http.addHeader("Content-Type", "application/json");  //Specify content-type header

    unsigned int httpCode = http.POST(buf);   //Send the request

    DEBUG_PRINT("HTTP Code: ");
    DEBUG_PRINTLN(httpCode);   //Print HTTP return code
    
    // No need to end if the system is being restarted because of deep sleep
    http.end();  //Close connection

    rval = true;
  }
  else
  {
    DEBUG_PRINTLN ("Not connected to WiFi - cant send to dweet.io");
  }

  return rval;
}

#endif
