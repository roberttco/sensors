#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>

#define BME280
#define DALLAS
#define DALLAS_GPIO 13

#define DEBUG
#define DEEP_SLEEP_TIME 120

#ifdef BME
#include <BME280I2C.h>
#include <EnvironmentCalculations.h>
#include <Wire.h>
BME280I2C::Settings settings(
   BME280::OSR_X4,
   BME280::OSR_X4,
   BME280::OSR_X4,
   BME280::Mode_Forced,
   BME280::StandbyTime_1000ms,
   BME280::Filter_Off,
   BME280::SpiEnable_False,
   0x76 // I2C address. I2C specific.
);

BME280I2C bme(settings);    // Default : forced mode, standby time = 1000 ms Oversampling = pressure ×1, temperature ×1, humidity ×1, filter off,

BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
BME280::PresUnit presUnit(BME280::PresUnit_hPa);
EnvironmentCalculations::TempUnit envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;
#endif

#ifdef DALLAS
#include <DS18B20.h>
DS18B20 ds(DALLAS_GPIO);
// Use the DS18B20 miltiple example to scan the bus and get the address
uint8_t address[] = {40,216,131,138,6,0,0,237};
uint8_t selected;
#endif

#define VCC_CORRECTION -0.25
ADC_MODE(ADC_VCC);

#define NUMWAPS 3
const char* ssid[NUMWAPS] = {"SensorNet","SensorNet_EXT","qsvMIMt8Fm6NV3"};
const char* password[NUMWAPS] = {"ypuGWK95fOpuOXoQmztz","ypuGWK95fOpuOXoQmztz","UbKNUJakLBLpOh"};
unsigned long wificonnecttime = 0L;
int whichap = -1;
long rssi;
int status = WL_IDLE_STATUS;

IPAddress ip    (192,168,2,90);
IPAddress dns   (192,168,2,5);
IPAddress gw    (192,168,2,1);
IPAddress subnet(255,255,255,0);

WiFiClient espClient;
WiFiUDP UDP;

char buf[1024];
float tempc;
float vdd;
unsigned long starttime;
unsigned long sleeptime;

#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)

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
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

// BEGIN CODE ===============================================================

int ConnectToWiFi(int retries, IPAddress ip, IPAddress dns, IPAddress gw, IPAddress subnet)
{
  int rval = -1;

  WiFi.mode(WIFI_STA);
  WiFi.config(ip, dns, gw, subnet);

  for (int wap = 0; wap < NUMWAPS; wap++)
  {
    int my_retries = retries;
    
    DEBUG_PRINT ("Attempting to connect to AP ");
    DEBUG_PRINT (ssid[wap]);

    WiFi.begin(ssid[wap], password[wap]);
    delay(250);
    status = WiFi.status();

    DEBUG_PRINT(", status: ");
    DEBUG_PRINTLN(StatusToString(status));
  
    // retry up to CONNECT_TIMEOUT seconds to connect
    while ((status != WL_CONNECTED) && ( my_retries > 0))
    {
      DEBUG_PRINT ("Attempting to connect to AP ");
      DEBUG_PRINT (ssid[wap]);
      
      // Connect to WPA/WPA2 network:
      
      status = WiFi.status();
      
      DEBUG_PRINT(", status: ");
      DEBUG_PRINTLN(StatusToString(status));
      
      delay(250);
      
      my_retries--;
    }
    
    if (status != WL_CONNECTED)
    {
      DEBUG_PRINTLN("Retries for this AP exhausted.  Moving on to next AP in one second.");
    }
    else
    {
      DEBUG_PRINT("IP address: ");
      DEBUG_PRINTLN(WiFi.localIP());
      rval = wap;
      break;
    }
  }

  return rval;
}

void setup()
{
#ifdef DEBUG
  Serial.begin(115200);
#endif

  DEBUG_PRINTLN ("Rolling...");
#ifdef BME
  Wire.pins(14, 13); // set the SDA and SCL pins to GPIO13 and 14
  Wire.begin();
  bme.begin();
#endif

#ifdef DALLAS
  selected = ds.select(address);

  if (selected == 0)
  {
    DEBUG_PRINTLN("No device found.  Restarting.");
    ESP.restart();
  }
#else
  selected = 0;
#endif

  starttime = millis();   

  // connect to wifi and retry each AP 20 times
  int retries = 1;  // retry once with a wifi wakeup in the middle
  while (retries > 0)
  {
    whichap = ConnectToWiFi(20,ip,dns,gw,subnet);
  
    wificonnecttime = millis() - starttime;   
    
    if (whichap == -1)
    {
      DEBUG_PRINTLN("Connect retries exhausted.  Forcing WiFi to wake up.");
      WiFi.forceSleepWake(); 
    }
    else
    {
      break;
    }
    
    retries--;
  }

  if (retries == 0)
  {
    DEBUG_PRINTLN("Connect retries exhausted.  Sleeping for 5 minutes.");
    ESP.deepSleep(300 * 1000000);
  }
  
  // get RSSI value including retries.
  DEBUG_PRINT("Getting RSSI ... ");
  
  retries = 5;
  rssi = WiFi.RSSI();
  while ((rssi > 0) && (retries > 0))
  {
    retries -= 1;
    delay(50);
  }

  if (retries <= 0)
  {
    rssi = 0;
  }
  
  DEBUG_PRINTLN(rssi);

  // get the BME280 readings
  float tempc(NAN), hum(NAN), pres(NAN);

#ifdef DALLAS
  if (selected)
  {
    tempc = ds.getTempC();
  }
#endif

#ifdef BME1820
  bme.read(pres, tempc, hum, tempUnit, presUnit);
#endif

  DEBUG_PRINTLN("Got values from sensor");

  // get the battery voltage
  vdd = (ESP.getVcc() / 1000.0) + VCC_CORRECTION;

  DEBUG_PRINTLN("Got battery voltage");

  float dewPoint = tempc - ((100 - hum) / 5);

  //DEBUG_PRINTLN("Take VCC reading now - you have 5 seconds.");
  // delay here to make it easier to read VCC with a volt meter to calibrate the VCC correction value
  //delay(5000);

  if (vdd < 3.0)
  {
    // if Vdd < 3 volts then go to sleep for 7 hours hoping for some sun
    DEBUG_PRINTLN ("Sleeping for 7 hours because Vdd is too low.");
    sleeptime = 3600 * 7 * 1E6;
  }
  else
  { 
    DEBUG_PRINTLN ("Sleeping for DEEP_SLEEP_TIME.");
    sleeptime = DEEP_SLEEP_TIME * 1E6;
  }

  snprintf(buf, sizeof(buf), "{ \"wifi\": { \"ssid\":\"%s\",\"connecttime\":\"%ld\", \"rssi\":\"%ld\" },"
                               "\"power\" : { \"vdd\": \"%f\", \"sleeptime\" : \"%ld\"},"
                               "\"environment\" : { \"tempc\":\"%f\", \"humidity\":\"%f\", \"pressure_hpa\":\"%f\", \"dewpoint\" : \"%f\" }}",
           ssid[whichap],
           wificonnecttime,
           rssi,
           vdd,
           sleeptime,
           tempc,
           hum,
           pres,
           dewPoint);

  //
  // DWEET
  //

  if (WiFi.status() == WL_CONNECTED)
  {
    DEBUG_PRINTLN("Sending to dweet.io");
    DEBUG_PRINTLN(buf);
    
    HTTPClient http;    //Declare object of class HTTPClient

    http.begin("http://dweet.io/dweet/quietly/for/ebd1425c-9026-452b-8178-7201bd977b55");      //Specify request destination
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
}

void loop()
{
  DEBUG_PRINTLN("Time to sleep...");
  ESP.deepSleep(sleeptime);
}
