#ifndef __MQTT_H
#define __MQTT_H

#include <PubSubClient.h>

#define MQTT_BROKER "192.168.2.6"
#define MQTT_PORT 24552
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""
#ifndef MQTT_TOPIC
#define MQTT_TOPIC "7133egyptian/out/sensors/sensor/json"
#endif

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

#endif // __MQTT_H
