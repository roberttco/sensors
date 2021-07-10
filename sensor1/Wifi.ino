#include "rtcdata.h"

IPAddress ip    (192,168,2,82);
IPAddress dns   (192,168,2,5);
IPAddress gw    (192,168,2,1);
IPAddress subnet(255,255,255,0);

#ifdef DEBUG

const char * StatusToString(int status)
{
  switch (status)
  {
    case WL_CONNECTED:
      return "Connected";
    case WL_NO_SHIELD:
      return "No network hardware";
    case WL_IDLE_STATUS:
      return "Waiting for connection";
    case WL_NO_SSID_AVAIL:
      return "No ssid available";
    case WL_SCAN_COMPLETED:
      return "WiFi scan completed";
    case WL_CONNECT_FAILED:
      return "Connection failed";
    case WL_CONNECTION_LOST:
      return "Connection lost";
    case WL_DISCONNECTED:
      return "Disconnected";
    default:
      return "Unknown WiFi status";
  }
}

#endif

String oneWireAddressToString(uint8_t *mac)
{
  char result[33];

  snprintf( result, sizeof( result ), "%d %d %d %d %d %d %d %d", 
   mac[ 0 ], mac[ 1 ], mac[ 2 ], mac[ 3 ], mac[ 4 ], mac[ 5 ], mac[6], mac[7] );

  return String( result );
}

String macAddressToString(uint8_t *mac)
{
  char result[14];

  snprintf( result, sizeof( result ), "%02X%02X%02X%02X%02X%02X", 
    mac[ 0 ], mac[ 1 ], mac[ 2 ], mac[ 3 ], mac[ 4 ], mac[ 5 ] );

  return String( result );
}

boolean ConnectToWiFi(RtcData *rtcData)
{
  boolean rval = false;
  int retries = 0;

  // the vlaue will be zero if the data isnt valid
  boolean apvalid = ((rtcData != NULL) && ((rtcData->valid & APVALID) == APVALID));
  
  Serial.printf("loops = %d, valid = %0x, apvalid = %d\n",loops,rtcData->valid,(apvalid ? 1 : 0));
  
  if((loops == 1) && (apvalid == true))
  {
    Serial.printf("loops = %d, valid = %0x\n",loops,rtcData->valid);
    // The RTC data was good, make a quick connection
    DEBUG_PRINTLN ("Connecting to AP using stored AP channel and MAC");
    WiFi.begin( ssid, password, rtcData->channel, rtcData->ap_mac, true );
  }
  else 
  {
    // The RTC data was not valid, so make a regular connection
    DEBUG_PRINTLN ("Connecting to AP by discovering AP channel and MAC");
    WiFi.begin( ssid, password );
  }
  delay(50);

  int wifiStatus = WiFi.status();
  while ( wifiStatus != WL_CONNECTED )
  {
    retries++;
    if( retries == 300 )
    {
      DEBUG_PRINTLN( "No connection after 300 steps, powercycling the WiFi radio. I have seen this work when the connection is unstable" );
      WiFi.disconnect();
      delay( 10 );
      WiFi.forceSleepBegin();
      delay( 10 );
      WiFi.forceSleepWake();
      delay( 10 );
      WiFi.begin( ssid, ssid );
    }

    if ( retries == 600 )
    {
      WiFi.disconnect( true );
      delay( 1 );
      WiFi.mode( WIFI_OFF );
      WiFi.forceSleepBegin();
      delay( 10 );
      
      if( loops == 3 )
      {
        DEBUG_PRINTLN( "That was 3 loops, still no connection so let's go to deep sleep for 2 minutes" );
        Serial.flush();
        ESP.deepSleep( 120000000, WAKE_RF_DISABLED );
      }
      else
      {
        DEBUG_PRINTLN( "No connection after 600 steps. WiFi connection failed." );
      }
        
      break;
    }
    delay( 50 );
    wifiStatus = WiFi.status();
  }

  if (wifiStatus == WL_CONNECTED)
  {
    rval= true;

    DEBUG_PRINT( "Connected to " );
    DEBUG_PRINTLN( ssid );
    DEBUG_PRINT( "Assigned IP address: " );
    DEBUG_PRINTLN( WiFi.localIP() );
  }

  return rval;
}

void ScanSsidsAndSend(String clientId, char *topic)
{
  char msg[1200];
  int msgRemain = 1200; // keep track of space to avoid overrunning memory;
  
  DEBUG_PRINTLN("Starting WiFi SSID Scan");

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  
  DEBUG_PRINTLN("scan done");
  
  if (n == 0) {
    DEBUG_PRINTLN("no networks found");
    strncat (msg, "{\"ap\":[]}", msgRemain);
    msgRemain -= 9;
  } else {
    char ssidstr[53];
    strncat (ssidstr,"{\"ap\":[",msgRemain);
    msgRemain -= 9; // 7 + closing ]}
    
    DEBUG_PRINT(n);
    DEBUG_PRINTLN(" networks found");
    for (int i = 0; i < min(20,n); ++i) {
      // Print SSID and RSSI for THE FIRST 20 networks found
      snprintf (ssidstr,sizeof(ssidstr),"{\"n\":\"%32s\",\"p\":\"%ld\"},",
        WiFi.SSID(i).c_str(),
        WiFi.RSSI(i));

      strncat(msg,ssidstr,msgRemain);
      msgRemain -= strlen(ssidstr);
      
      DEBUG_PRINT((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      DEBUG_PRINTLN(ssidstr);
    }
    strncat(msg,"]}",msgRemain);
  }

  MQTT_ConnectAndSend(clientId, topic, msg);
}
