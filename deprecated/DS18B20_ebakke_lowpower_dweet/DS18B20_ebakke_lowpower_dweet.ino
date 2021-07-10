#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <DS18B20.h>
#include <rtc_memory.h>

#include "rtcdata.h"

// make it so the A/D converter reads VDD
ADC_MODE(ADC_VCC);

#define DEBUG
//#define DEBUG_ESP_HTTP_UPDATE

#define FW_VERSION 24

// #define CALIBRATE_VCC
#define VCC_CORRECTION -0.25
#define VCC_CUTOFF 2.00

#define SENSOR1
//#define SENSOR2

#ifdef SENSOR1
#define DWEET_URL "http://dweet.io/dweet/quietly/for/ebd1425c-9026-452b-8178-7201bd977b55"
#define DALLAS_GPIO 13
#define DEEP_SLEEP_TIME_SECONDS 300
#endif

#ifdef SENSOR2
#define DWEET_URL "http://dweet.io/dweet/quietly/for/c440d18e-4fa7-4eb7-bafb-499b19de97ab"
#define DALLAS_GPIO 12
#define DEEP_SLEEP_TIME_SECONDS 60
#endif

// #########################
// # WIFI variables
// #########################
//#define NUMWAPS 3
//const char* ssid[NUMWAPS] = {"SensorNet","SensorNet_EXT","qsvMIMt8Fm6NV3"};
//const char* password[NUMWAPS] = {"ypuGWK95fOpuOXoQmztz","ypuGWK95fOpuOXoQmztz","UbKNUJakLBLpOh"};
const char * password = "UbKNUJakLBLpOh";
const char * ssid = "qsvMIMt8Fm6NV3";

unsigned long wificonnecttime = 0L;
int status = WL_IDLE_STATUS;

WiFiClient wifiClient;

// #########################
// # DS18B20 variables
// #########################
// Use the DS18B20 miltiple example to scan the bus and get the address
DS18B20 ds(DALLAS_GPIO);
uint8_t ds18b20address[8];// = {40,216,131,138,6,0,0,237};
uint8_t selected;

RtcMemory rtcMemory;

// #########################
// # OTA update variables
// #########################
const int i_firmwareVersion = FW_VERSION;
const char* fwUrlBase = "http://192.168.2.5/firmware/";

// #########################
// # Misc variables
// #########################
char buf[1024];
float tempc;
double ucvdd;
double vdd;
int loops = 0;
RtcData* rtcData = NULL;

unsigned long starttime;
unsigned long sleepTimeSeconds = DEEP_SLEEP_TIME_SECONDS;

// #########################
// # Declarations
// #########################

// #########################
// # Debug stuff
// #########################
#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

void setup()
{
  pinMode(4, INPUT_PULLUP);
  
  starttime = millis();   

#ifdef DEBUG
  Serial.begin(115200);
  while ( !Serial );
#endif

  // start with WiFi turned off
  // https://www.bakke.online/index.php/2017/05/21/reducing-wifi-power-consumption-on-esp8266-part-2/
  WiFi.mode( WIFI_OFF );
  WiFi.forceSleepBegin();
  delay( 1 );

  DEBUG_PRINTLN ("\nRolling...");

  if(rtcMemory.begin()){
    DEBUG_PRINTLN("RTC memory library initialization done!");
  } else {
    DEBUG_PRINTLN("No previous RTC memory data found. The memory is reset to zeros!");
  }

  // Get the data
  rtcData = rtcMemory.getData<RtcData>();

#ifdef DEBUG
  Serial.printf("Valid: %1X\nChannel: %d\nap_mac: %s\nds1820addr: %s\n",
    rtcData->valid,
    rtcData->channel,
    macAddressToString(rtcData->ap_mac).c_str(),
    oneWireAddressToString(rtcData->ds1820addr).c_str());
#endif

  // if gpio 4 is low, then wope the RTC data and force a rediscovery of the AP and the DS18B20 address
  int sensorVal = digitalRead(4);
  if (sensorVal == 0)
  {
    DEBUG_PRINTLN("Clearing RTC data structure.");
    memset(rtcData,0,sizeof(RtcData));
  }
}

void loop()
{
  loops++;

  // get the battery voltage
  ucvdd = ESP.getVcc() / 1000.0;
  vdd = ucvdd + VCC_CORRECTION;

#ifdef DEBUG
  Serial.printf ("Uncorrected VDD %f mV. Corrected to %f mV.\n",ucvdd,vdd);
#endif

#ifdef CALIBRATE_VCC
  DEBUG_PRINTLN("Take VCC reading now - you have 5 seconds.");
  //delay here to make it easier to read VCC with a volt meter to calibrate the VCC correction value
  delay(5000);
#endif

  if(rtcData != NULL && (rtcData->valid & DSVALID == DSVALID)) 
  {
    DEBUG_PRINTLN("Using stored DS18B0 address.");
    selected = ds.select(rtcData->ds1820addr);
  }
  else
  {
    DEBUG_PRINTLN("Getting DS18B0 address (and storing it for next time).");
    while (selected == 0 && ds.selectNext()) 
    {
#ifdef DEBUG      
      switch (ds.getFamilyCode())
      {
        case MODEL_DS18S20:
          Serial.println("Model: DS18S20/DS1820");
          break;
        case MODEL_DS1822:
          Serial.println("Model: DS1822");
          break;
        case MODEL_DS18B20:
          Serial.println("Model: DS18B20");
          break;
        default:
          Serial.println("Unrecognized Device");
          break;
      }
#endif

      uint8_t ds18b20address[8];
      ds.getAddress(ds18b20address);
      selected = 1;

      DEBUG_PRINTLN(oneWireAddressToString(ds18b20address));

      rtcData->valid |= DSVALID;
      memcpy( rtcData->ds1820addr, ds18b20address, 8 ); // Copy the DS18B20 address to memory
      break;
    }
  }

  if (selected != 0)
  {
    tempc = ds.getTempC();
    DEBUG_PRINTLN("Got temperature");
  }
  else
  {
    tempc = 255;
    DEBUG_PRINTLN("Could not get temperature");
  }

  // Now send out the measurements
  
  int connectRetries = 1;  // retry once with a wifi wakeup in the middle
  String myMac;
  while (connectRetries > 0)
  {
    if (ConnectToWiFi(rtcData))
    {
      // successfully connected
      uint8_t mac[6];
      WiFi.macAddress(mac);
      myMac = macAddressToString(mac);

      wificonnecttime = millis() - starttime;   
      break;

      
    }
    
    connectRetries--;
  }

  if (connectRetries == 0)
  {
    DEBUG_PRINTLN("Connect retries exhausted.  Sleeping for 5 minutes.");
    sleepTimeSeconds = 300;
  }
  else
  {
    // get RSSI value including retries.
    DEBUG_PRINT("Getting RSSI ... ");
    
    connectRetries = 5;
    long rssi = WiFi.RSSI();
    while ((rssi > 0) && (connectRetries > 0))
    {
      connectRetries--;
      delay(50);
    }
  
    if (connectRetries <= 0)
    {
      rssi = 0;
    }
    
    DEBUG_PRINTLN(rssi);
  
    snprintf(buf, sizeof(buf), "{ \"wifi\": { \"ssid\":\"%s\",\"connecttime\":\"%ld\", \"rssi\":\"%ld\",\"mac\":\"%s\" },"
                                 "\"power\" : { \"ucvdd\": \"%.2f\", \"vdd\": \"%.2f\", \"sleepTimeSeconds\" : \"%ld\"},"
                                 "\"environment\" : { \"tempc\":\"%f\", \"humidity\":\"%f\", \"pressure_hpa\":\"%f\", \"dewpoint\" : \"%f\" },"
                                 "\"firmware\" : { \"version\":\"%d\",\"date\":\"%s %s\" }}",
                                
             ssid,
             wificonnecttime,
             rssi,
             myMac.c_str(),
             
             ucvdd,
             vdd,
             sleepTimeSeconds,
             
             tempc,
             0.0,
             0.0,
             0.0,
             
             i_firmwareVersion,
             __DATE__,
             __TIME__);
  
    //
    // DWEET
    //
  
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
    }
    else
    {
      DEBUG_PRINTLN ("Not connected to WiFi - cant send to dweet.io");
    }
  
    checkForUpdates(myMac);

  }

  rtcMemory.save();
  
#ifdef DEBUG
  Serial.printf ("Total runtime: %ld\n",millis() - starttime);
#endif

  DEBUG_PRINTLN("\n=====\nTime to sleep...");
  WiFi.disconnect( true );
  delay( 1 );

  if (vdd < VCC_CUTOFF)
  {
    // if Vdd < 3 volts then go to sleep for 1 hour hoping for some sun
    DEBUG_PRINTLN ("Sleeping for 20 minutes because Vdd is too low - hoping for some sun.");
    sleepTimeSeconds = 1200;
  }
  else
  { 
    sleepTimeSeconds = DEEP_SLEEP_TIME_SECONDS;
  }

  // WAKE_RF_DISABLED to keep the WiFi radio disabled when we wake up
#ifdef DEBUG
  Serial.printf("Sleeping for %i seconds.\n",sleepTimeSeconds);
#endif
  
  ESP.deepSleep(sleepTimeSeconds * 1e6, WAKE_RF_DISABLED );
}
