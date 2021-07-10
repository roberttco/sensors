#include <ESP8266WiFi.h>
#include <DS18B20.h>
#include "rtcdata.h"

#define FW_VERSION 37
#include "ota.h"

// sensor 1
// Device is an ESP-07
// Choose Generic ESP8266 with 1MB RAM (FS:none, OTA:502KB)

#define DALLAS_GPIO 13
#define DEEP_SLEEP_TIME_SECONDS 300
#define SSID_RESCANLOOPS 1
#define MQTT_TOPIC "7133egyptian/out/sensors/sensor1/json"
#define SSID_TOPIC "7133egyptian/out/sensors/sensor1/ssids"
#define DWEET_URL "http://dweet.io/dweet/quietly/for/ebd1425c-9026-452b-8178-7201bd977b55"

// sensor 2
//#define DALLAS_GPIO 12
//#define DEEP_SLEEP_TIME_SECONDS 60
//#define MQTT_TOPIC "7133egyptian/out/sensors/sensor2/json"
//#define DWEET_URL "http://dweet.io/dweet/quietly/for/c440d18e-4fa7-4eb7-bafb-499b19de97ab"

#include "dweet.h"   // comment this out to EXCLUDE dweet.io publishing
#include "mqtt.h"    // comment this out to EXCLUDE mqtt publishing

// make it so the A/D converter reads VDD
ADC_MODE(ADC_VCC);

#define DEBUG
//#define DEBUG_ESP_HTTP_UPDATE

// #define CALIBRATE_VCC
#define VCC_CORRECTION -0.25
#define VCC_CUTOFF 2.00

// #########################
// # WIFI variables
// #########################
//const char * password = "UbKNUJakLBLpOh";
//const char * ssid = "qsvMIMt8Fm6NV3";
const char * password = "5ms21Od55yy5zgJERTYK";
const char * ssid = "Singularity";

unsigned long wificonnecttime = 0L;
int status = WL_IDLE_STATUS;

// #########################
// # DS18B20 variables
// #########################
// Use the DS18B20 miltiple example to scan the bus and get the address
DS18B20 ds(DALLAS_GPIO);
uint8_t ds18b20address[8];// = {40,216,131,138,6,0,0,237};
uint8_t selected;

RtcMemory rtcMemory;

// #########################
// # Misc variables
// #########################
char buf[1024];
float tempc;
double ucvdd;
double vdd;
int loops = 0;
RtcData* rtcData = NULL;
String myMac;
unsigned long starttime;
unsigned long sleepTimeSeconds = DEEP_SLEEP_TIME_SECONDS;

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
  Serial.printf("Valid: %X\nChannel: %d\nap_mac: %s\nds1820addr: %s\nLoops before scan: %X",
    rtcData->valid,
    rtcData->channel,
    macAddressToString(rtcData->ap_mac).c_str(),
    oneWireAddressToString(rtcData->ds1820addr).c_str(),
    rtcData->loopsBeforeScan);
#endif

  // if gpio 13 is low, then wipe the RTC data and force a rediscovery of the AP and the DS18B20 address
  int clear_a = digitalRead(13);
  int clear_b = digitalRead(14);
  if ((clear_a == 0) && (clear_b == 0))
  {
    DEBUG_PRINTLN("Clearing RTC data structure.");
    memset(rtcData,0,sizeof(RtcData));

    // after clearing the RTC memory, wait 10 secoinds and reboot so there is time to
    // reconnect the sensor wire.
    delay(10000);
    ESP.restart();
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

  if(rtcData != NULL && ((rtcData->valid & DSVALID) == DSVALID)) 
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
  while (connectRetries > 0)
  {
    if (ConnectToWiFi(rtcData))
    {
      // successfully connected
      uint8_t mac[6];
      WiFi.macAddress(mac);
      myMac = macAddressToString(mac);

      // Write current connection info back to RTC
      rtcData->channel = WiFi.channel();
      rtcData->valid |= APVALID;
      memcpy( rtcData->ap_mac, WiFi.BSSID(), 6 ); // Copy 6 bytes of BSSID (AP's MAC address)

      wificonnecttime = millis() - starttime;   
      break;
    }
    else
    {
      // wait 10 second between retries.
      delay(10000);
    }
    
    connectRetries--;
  }

  if (connectRetries == 0)
  {
    DEBUG_PRINTLN("Connect retries exhausted.  Clearing RTC data and sleeping for 5 minutes.");

    // clear RTC data in case there is something amiss with the info - get a fresh start.
    memset(rtcData,0,sizeof(RtcData));
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
                                 "\"power\" : { \"ucvdd\": \"%.2f\", \"vdd\": \"%.2f\", \"sleepTimeSeconds\" : \"%ld\", \"loopsBeforeSsidScan\" : \"%ld\"},"
                                 "\"environment\" : { \"tempc\":\"%f\", \"humidity\":\"%f\", \"pressure_hpa\":\"%f\", \"dewpoint\" : \"%f\" },"
                                 "\"firmware\" : { \"version\":\"%d\",\"date\":\"%s %s\" }}",
                                
             ssid,
             wificonnecttime,
             rssi,
             myMac.c_str(),
             
             ucvdd,
             vdd,
             sleepTimeSeconds,
             rtcData->loopsBeforeScan,
                          
             tempc,
             0.0,
             0.0,
             0.0,
             
             i_firmwareVersion,
             __DATE__,
             __TIME__);
    
    uint16_t bufLen = (uint16_t)strlen(buf);
      
    if (WiFi.status() == WL_CONNECTED)
    {
#ifdef __DWEET_H
      DWEET_ConnectAndSend(buf);
#endif 

#ifdef __MQTT_H
      MQTT_ConnectAndSend(myMac, MQTT_TOPIC, buf);

      if (rtcData->loopsBeforeScan <= 0)
      {
        ScanSsidsAndSend(myMac, SSID_TOPIC);
        rtcData->loopsBeforeScan = SSID_RESCANLOOPS;
      }
      else
      {
        rtcData->loopsBeforeScan = rtcData->loopsBeforeScan - 1;
      }
#endif
  
      checkForUpdates(myMac);
    }
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
