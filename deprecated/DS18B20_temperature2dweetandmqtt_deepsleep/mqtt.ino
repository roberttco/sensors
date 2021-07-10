bool pubSubReconnect() {
  // Loop until we're reconnected
  int retries = 3;
  while (!pubSubClient.connected() && retries > 0) {
    DEBUG_PRINT("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (pubSubClient.connect(clientId.c_str())) {
      DEBUG_PRINTLN("connected");
      
      return true;
    } else {
      DEBUG_PRINT("mqtt broker connect failed, rc=");
      DEBUG_PRINT(pubSubClient.state());
      DEBUG_PRINTLN(" try again in 1 seconds");
      // Wait 5 seconds before retrying
      delay(1000);
      retries--;
    }
  }
  return false;
}

void pubSubCallback(char* topic, byte* payload, unsigned int length) {
  DEBUG_PRINT("Message arrived [");
  DEBUG_PRINT(topic);
  DEBUG_PRINT("] ");
  for (int i = 0; i < length; i++) {
    DEBUG_PRINT((char)payload[i]);
  }
  DEBUG_PRINTLN();
}

void publishToMqtt(const char *buf)
{
  bool pubSubConnected = false;
  if (!pubSubClient.connected()) 
  {
    pubSubConnected = pubSubReconnect();

    if (pubSubConnected == true)
    {
      // Once connected, publish an announcement...
      pubSubClient.publish  ("7133egyptian/out/sensors/sensor1/state","ONLINE");
      pubSubClient.publish  ("7133egyptian/out/sensors/sensor1/json", buf);
      pubSubClient.subscribe("7133egyptian/in/sensors/sensor1/command");
    }
  }
  
  for (int i = 0 ; i < 20 ; i++)
  {
    pubSubClient.loop();
  }
}
